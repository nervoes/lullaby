/*
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "lullaby/systems/physics/physics_system.h"

#include "lullaby/events/entity_events.h"
#include "lullaby/events/physics_events.h"
#include "lullaby/modules/flatbuffers/mathfu_fb_conversions.h"
#include "lullaby/systems/dispatcher/event.h"
#include "lullaby/systems/transform/transform_system.h"
#include "lullaby/util/logging.h"
#include "lullaby/util/make_unique.h"
#include "lullaby/util/time.h"
#include "lullaby/util/trace.h"
#include "lullaby/generated/physics_shapes_generated.h"

namespace lull {

const HashValue kRigidBodyDef = Hash("RigidBodyDef");

PhysicsSystem::PhysicsSystem(Registry* registry, const mathfu::vec3& gravity)
    : System(registry),
      rigid_bodies_(16),
      transform_system_(nullptr),
      transform_flag_(TransformSystem::kInvalidFlag),
      bt_config_(MakeUnique<btDefaultCollisionConfiguration>()),
      bt_dispatcher_(MakeUnique<btCollisionDispatcher>(bt_config_.get())),
      bt_broadphase_(MakeUnique<btDbvtBroadphase>()),
      bt_solver_(MakeUnique<btSequentialImpulseConstraintSolver>()),
      bt_world_(MakeUnique<btDiscreteDynamicsWorld>(bt_dispatcher_.get(),
                                                    bt_broadphase_.get(),
                                                    bt_solver_.get(),
                                                    bt_config_.get())) {
  RegisterDef(this, kRigidBodyDef);
  RegisterDependency<DispatcherSystem>(this);
  RegisterDependency<TransformSystem>(this);

  bt_world_->setGravity(BtVectorFromMathfu(gravity));
  bt_world_->setInternalTickCallback(
      InternalTickCallback, static_cast<void*>(this));
}

PhysicsSystem::~PhysicsSystem() {
  auto* dispatcher = registry_->Get<Dispatcher>();
  if (dispatcher) {
    dispatcher->DisconnectAll(this);
  }
  auto* transform_system = registry_->Get<TransformSystem>();
  if (transform_system && transform_flag_ != TransformSystem::kInvalidFlag) {
    transform_system->ReleaseFlag(transform_flag_);
  }
}


void PhysicsSystem::Initialize() {
  transform_system_ = registry_->Get<TransformSystem>();
  transform_flag_ = transform_system_->RequestFlag();

  auto* dispatcher = registry_->Get<Dispatcher>();
  dispatcher->Connect(this, [this](const OnDisabledEvent& event) {
    OnEntityDisabled(event.target);
  });
  dispatcher->Connect(this, [this](const OnEnabledEvent& event) {
    OnEntityEnabled(event.target);
  });
  dispatcher->Connect(this, [this](const ParentChangedEvent& event) {
    OnParentChanged(event.target, event.new_parent);
  });
}

void PhysicsSystem::Create(Entity entity, HashValue type, const Def* def) {
  if (type == kRigidBodyDef) {
    rigid_bodies_.Emplace(entity);
  } else {
    LOG(DFATAL) << "Unsupported ComponentDef type: " << type;
  }
}

void PhysicsSystem::PostCreateInit(
    Entity entity, HashValue type, const Def* def) {
  if (type == kRigidBodyDef) {
    const RigidBodyDef* data = ConvertDef<RigidBodyDef>(def);
    InitRigidBody(entity, data);
  } else {
    LOG(DFATAL) << "Unsupported ComponentDef type: " << type;
  }
}

void PhysicsSystem::Destroy(Entity entity) {
  DisablePhysics(entity);
  rigid_bodies_.Destroy(entity);
}

void PhysicsSystem::InitRigidBody(Entity entity, const RigidBodyDef* data) {
  auto* body = rigid_bodies_.Get(entity);
  if (!body) {
    LOG(DFATAL) << "Couldn't find a rigid body.";
    return;
  }

  // Dynamic rigid bodies must be top-level Entities.
  const RigidBodyType type = data->type();
  if (type == RigidBodyType::RigidBodyType_Dynamic) {
    CHECK(transform_system_->GetParent(entity) == kNullEntity)
        << "Dynamic rigid bodies cannot have parents.";
  }

  // Get the world SQT for the Entity to initialize the rigid body's transform.
  const mathfu::mat4* world_from_entity_mat =
      transform_system_->GetWorldFromEntityMatrix(entity);
  if (!world_from_entity_mat) {
    LOG(DFATAL) << "No world matrix for entity.";
    return;
  }

  // Ensure that center of mass transforms are applied in local space.
  MathfuVec3FromFbVec3(data->center_of_mass_translation(),
                       &body->center_of_mass_translation);
  const mathfu::mat4 simulation_mat =
      *world_from_entity_mat *
      mathfu::mat4::FromTranslationVector(body->center_of_mass_translation);
  const Sqt sqt = CalculateSqtFromMatrix(simulation_mat);

  // Check for shear (not explicitly supported in Lullaby, but occurs if
  // non-uniform scales are used in the transforms between the given Entity and
  // the root).
  CHECK(MatrixAlmostOrthogonal(sqt.rotation.ToMatrix(), 1e-3f))
      << "Incoming matrix has shear components.";

  // Apply the transform as a translation + rotation and set up the collision
  // shape. Scale will be applied directly to the collision shape.
  const btTransform xform(BtQuatFromMathfu(sqt.rotation),
                          BtVectorFromMathfu(sqt.translation));
  SetupCollisionShape(body, data, sqt.scale);

  // Set up standard rigid body parameters. Zero mass Entities will be treated
  // by Bullet as static.
  const float mass =
      type == RigidBodyType::RigidBodyType_Static ? 0.f : data->mass();
  const bool is_dynamic = (type == RigidBodyType::RigidBodyType_Dynamic);

  // Calculate the local inertia for dynamic objects.
  btVector3 local_inertia(0.f, 0.f, 0.f);
  if (is_dynamic) {
    body->bt_primary_shape->calculateLocalInertia(mass, local_inertia);
  }

  // Setup the motion state and construction info struct, build the rigid body,
  // and set it's transform flag.
  body->type = type;
  body->bt_motion_state = MakeUnique<MotionState>(xform, this, entity);

  btRigidBody::btRigidBodyConstructionInfo construction_info(
      mass, body->bt_motion_state.get(), body->bt_primary_shape, local_inertia);
  construction_info.m_friction = data->friction();
  construction_info.m_restitution = data->restitution();

  body->bt_body = MakeUnique<btRigidBody>(construction_info);
  transform_system_->SetFlag(entity, transform_flag_);

  // Setup kinematic body collision flags - static bodies are handled by Bullet.
  if (type == RigidBodyType::RigidBodyType_Kinematic) {
    body->bt_body->setCollisionFlags(
        body->bt_body->getCollisionFlags()
        | btCollisionObject::CollisionFlags::CF_KINEMATIC_OBJECT);
  }

  // Hack triggers. TODO(birenbaum): Remove/replace this once proper triggers
  // are implemented; cl/164340709.
  body->bt_body->setCollisionFlags(
      body->bt_body->getCollisionFlags() |
      btCollisionObject::CollisionFlags::CF_NO_CONTACT_RESPONSE);

  // Give the rigid body a pointer to the Lullaby entity.
  body->bt_body->setUserPointer(reinterpret_cast<void*>(entity));

  // Enable the entity, putting it into the physics world.
  if (data->enable_on_create()) {
    EnablePhysics(entity);
  }

  // Setup initial momentum states.
  if (data->linear_velocity()) {
    mathfu::vec3 linear_velocity;
    MathfuVec3FromFbVec3(data->linear_velocity(), &linear_velocity);
    SetLinearVelocity(entity, linear_velocity);
  }
  if (data->angular_velocity()) {
    mathfu::vec3 angular_velocity;
    MathfuVec3FromFbVec3(data->angular_velocity(), &angular_velocity);
    SetAngularVelocity(entity, angular_velocity);
  }
}

void PhysicsSystem::SetupCollisionShape(RigidBody* body,
                                        const RigidBodyDef* data,
                                        const mathfu::vec3& entity_scale) {
  // If the shape list is empty, fall back to using the AABB of the shape.
  const auto* shape_parts = data->shapes();
  if (shape_parts == nullptr || shape_parts->size() < 1) {
    // TODO(b/64477700): use the AABB as a backup.
    LOG(DFATAL) << "A body must have a shape.";
    return;
  }

  // If there is only one shape, it may be possible to just use that shape
  // instead of requiring a compound shape.
  const auto num_shapes = shape_parts->size();
  if (num_shapes == 1) {
    const PhysicsShapePart* part = (*shape_parts)[0];
    const Sqt shape_sqt = GetShapeSqt(part);
    std::unique_ptr<btCollisionShape> shape = CreateCollisionShape(part);
    shape->setLocalScaling(BtVectorFromMathfu(shape_sqt.scale));

    // If no local transforms are applied, make this shape the primary shape and
    // avoid using a compound shape altogether.
    const float rotation_delta =
        1.f -
        mathfu::quat::DotProduct(shape_sqt.rotation, mathfu::quat::identity);
    if (shape_sqt.translation == mathfu::kZeros3f &&
        std::fabs(rotation_delta) <= kDefaultEpsilon) {
      body->bt_primary_shape = shape.get();
      body->bt_shapes.emplace_back(std::move(shape));
    } else {
      // Otherwise, create a compound, add the shape to it with the transform,
      // and use that as the primary.
      auto compound = MakeUnique<btCompoundShape>(true /* dynamic AABB tree */,
                                                  1 /* initial size */);
      body->bt_primary_shape = compound.get();

      const btTransform transform(BtQuatFromMathfu(shape_sqt.rotation),
                                  BtVectorFromMathfu(shape_sqt.translation));
      compound->addChildShape(transform, shape.get());
      body->bt_shapes.emplace_back(std::move(shape));

      body->bt_shapes.emplace_back(std::move(compound));
    }
  } else {
    // Otherwise, create a compound shape and populate it with the other shapes.
    auto compound = MakeUnique<btCompoundShape>(true /* dynamic AABB tree */,
                                                num_shapes /* initial size */);
    body->bt_primary_shape = compound.get();
    for (const auto* part : *shape_parts) {
      Sqt shape_sqt = GetShapeSqt(part);
      auto shape = CreateCollisionShape(part);
      shape->setLocalScaling(BtVectorFromMathfu(shape_sqt.scale));

      const btTransform transform(BtQuatFromMathfu(shape_sqt.rotation),
                                  BtVectorFromMathfu(shape_sqt.translation));
      compound->addChildShape(transform, shape.get());
      body->bt_shapes.emplace_back(std::move(shape));
    }

    // Finally, store the compound as well to ensure it is cleaned up.
    body->bt_shapes.emplace_back(std::move(compound));
  }

  // Apply the Entity's scaling on top of individual collision shape scaling.
  body->bt_primary_shape->setLocalScaling(
      body->bt_primary_shape->getLocalScaling() *
      BtVectorFromMathfu(entity_scale));
}

void PhysicsSystem::InternalTickCallback(
    btDynamicsWorld* world, btScalar time_step) {
  PhysicsSystem* owner = static_cast<PhysicsSystem*>(world->getWorldUserInfo());
  owner->PostSimulationTick();
}

void PhysicsSystem::PostSimulationTick() {
  // Retrieve all the manifolds from the most recent tick and collect all the
  // new contacts.
  auto* dispatcher_system = registry_->Get<DispatcherSystem>();
  ContactMap new_contacts;
  const int num_manifolds = bt_dispatcher_->getNumManifolds();
  for (int i = 0; i < num_manifolds; ++i) {
    // Grab the two collision objects and their rigid bodies from the manifold.
    // A persistent manifold exists for two Entities as long as they are still
    // colliding in the broadphase, so it may be empty, indicating that the
    // objects are no longer colliding.
    const btPersistentManifold* contact_manifold =
        bt_dispatcher_->getManifoldByIndexInternal(i);
    if (contact_manifold->getNumContacts() == 0) {
      continue;
    }

    const btCollisionObject* obj1 = contact_manifold->getBody0();
    const Entity entity1 =
        static_cast<Entity>(reinterpret_cast<uint64_t>(obj1->getUserPointer()));

    const btCollisionObject* obj2 = contact_manifold->getBody1();
    const Entity entity2 =
        static_cast<Entity>(reinterpret_cast<uint64_t>(obj2->getUserPointer()));

    // If both rigid bodies exist, check if there is already contact between the
    // two. If the contact is new, send events to both Entities. Contacts are
    // stored for the lowest valued Entity.
    Entity primary;
    Entity secondary;
    PickPrimaryAndSecondaryEntities(entity1, entity2, &primary, &secondary);
    new_contacts[primary].insert(secondary);

    if (dispatcher_system && !AreInContact(primary, secondary)) {
      dispatcher_system->Send(primary, EnterPhysicsContactEvent(secondary));
      dispatcher_system->Send(secondary, EnterPhysicsContactEvent(primary));
    }
  }

  // Check which contacts no longer exist.
  if (dispatcher_system) {
    for (const auto& current : current_contacts_) {
      const Entity primary = current.first;
      for (const auto& secondary : current.second) {
        auto contact = new_contacts.find(primary);
        if (contact == new_contacts.end()
           || contact->second.find(secondary) == contact->second.end()) {
          dispatcher_system->Send(primary, ExitPhysicsContactEvent(secondary));
          dispatcher_system->Send(secondary, ExitPhysicsContactEvent(primary));
        }
      }
    }
  }

  using std::swap;
  std::swap(current_contacts_, new_contacts);
}

void PhysicsSystem::UpdateSimulationTransform(
    Entity entity, const mathfu::mat4& world_from_entity_mat) {
  auto* body = rigid_bodies_.Get(entity);
  if (body->type == RigidBodyType::RigidBodyType_Static) {
    return;
  }

  // Convert the matrix to a SQT to ensure that scale is extracted before the
  // rotation matrix is calculated. Also make sure that local offset transforms
  // are applied in local space.
  const mathfu::mat4 simulation_mat =
      world_from_entity_mat *
      mathfu::mat4::FromTranslationVector(body->center_of_mass_translation);
  const Sqt sqt = CalculateSqtFromMatrix(simulation_mat);

  btTransform transform = body->bt_body->getWorldTransform();
  transform.setOrigin(BtVectorFromMathfu(sqt.translation));
  transform.setRotation(BtQuatFromMathfu(sqt.rotation));

  if (body->type == RigidBodyType::RigidBodyType_Dynamic) {
    body->bt_body->proceedToTransform(transform);
  } else if (body->type == RigidBodyType::RigidBodyType_Kinematic) {
    body->bt_motion_state->SetKinematicTransform(transform);
  }
  body->bt_body->activate(true);

  // Ensure that local scaling is also applied, but only do so if it changes
  // since this operation can be expensive.
  btVector3 local_scale = BtVectorFromMathfu(sqt.scale);
  if (local_scale != body->bt_primary_shape->getLocalScaling()) {
    body->bt_primary_shape->setLocalScaling(local_scale);
  }
}

void PhysicsSystem::UpdateLullabyTransform(Entity entity) {
  auto* body = rigid_bodies_.Get(entity);
  // Only Dynamic bodies are simulated by Bullet.
  if (body->type != RigidBodyType::RigidBodyType_Dynamic) {
    return;
  }

  // Un-apply any local offset transforms.
  const btTransform& world_transform = body->bt_body->getWorldTransform();
  Sqt sqt(MathfuVectorFromBt(world_transform.getOrigin()),
          MathfuQuatFromBt(world_transform.getRotation()),
          transform_system_->GetLocalScale(entity));
  sqt.translation =
      CalculateTransformMatrix(sqt) * -body->center_of_mass_translation;
  transform_system_->SetSqt(entity, sqt);
}

void PhysicsSystem::AdvanceFrame(Clock::duration delta_time) {
  LULLABY_CPU_TRACE_CALL();

  transform_system_->ForEach(
      transform_flag_,
      [this](Entity e, const mathfu::mat4& world_from_entity_mat,
             const Aabb& box) {
        UpdateSimulationTransform(e, world_from_entity_mat);
      });

  // Update the simulation MotionStates will take care of exporting the
  // transforms to the TransformSystem for bodies that are Dynamic, which are
  // the only kind that Bullet will update.
  const float delta_time_sec = SecondsFromDuration(delta_time);
  bt_world_->stepSimulation(delta_time_sec);
}

void PhysicsSystem::PickPrimaryAndSecondaryEntities(
      Entity one, Entity two, Entity* primary, Entity* secondary) {
  *primary =  one < two ? one : two;
  *secondary = one < two ? two : one;
}

bool PhysicsSystem::AreInContact(Entity one, Entity two) const {
  Entity primary;
  Entity secondary;
  PickPrimaryAndSecondaryEntities(one, two, &primary, &secondary);

  auto iter = current_contacts_.find(primary);
  return iter != current_contacts_.end()
      && iter->second.find(secondary) != iter->second.end();
}

void PhysicsSystem::SetLinearVelocity(Entity entity,
                                      const mathfu::vec3& velocity) {
  auto* body = rigid_bodies_.Get(entity);
  if (!IsPhysicsEnabled(entity)
      || body->type != RigidBodyType::RigidBodyType_Dynamic) {
    return;
  }

  body->bt_body->setLinearVelocity(BtVectorFromMathfu(velocity));
  body->bt_body->activate(true);
}

void PhysicsSystem::SetAngularVelocity(Entity entity,
                                       const mathfu::vec3& velocity) {
  auto* body = rigid_bodies_.Get(entity);
  if (!IsPhysicsEnabled(entity)
      || body->type != RigidBodyType::RigidBodyType_Dynamic) {
    return;
  }

  body->bt_body->setAngularVelocity(BtVectorFromMathfu(velocity));
  body->bt_body->activate(true);
}

void PhysicsSystem::DisablePhysics(Entity entity) {
  if (IsPhysicsEnabled(entity)) {
    auto* body = rigid_bodies_.Get(entity);
    bt_world_->removeRigidBody(body->bt_body.get());
    body->enabled = false;
    transform_system_->ClearFlag(entity, transform_flag_);
  }
}

void PhysicsSystem::EnablePhysics(Entity entity) {
  if (!IsPhysicsEnabled(entity)) {
    auto* body = rigid_bodies_.Get(entity);
    bt_world_->addRigidBody(body->bt_body.get());
    body->enabled = true;
    transform_system_->SetFlag(entity, transform_flag_);
  }
}

bool PhysicsSystem::IsPhysicsEnabled(Entity entity) const {
  auto* body = rigid_bodies_.Get(entity);
  return (body && body->enabled);
}

void PhysicsSystem::OnEntityDisabled(Entity entity) {
  auto* body = rigid_bodies_.Get(entity);
  if (body && body->enabled) {
    bt_world_->removeRigidBody(body->bt_body.get());
  }
}

void PhysicsSystem::OnEntityEnabled(Entity entity) {
  auto* body = rigid_bodies_.Get(entity);
  if (body && body->enabled) {
    bt_world_->addRigidBody(body->bt_body.get());
  }
}

void PhysicsSystem::OnParentChanged(Entity entity, Entity new_parent) {
  auto* body = rigid_bodies_.Get(entity);
  if (body && body->type == RigidBodyType::RigidBodyType_Dynamic) {
    CHECK(new_parent == kNullEntity) << "Dynamic bodies cannot have parents.";
  }
}

// Implementation of btMotionState.

void PhysicsSystem::MotionState::getWorldTransform(
    btTransform& world_transform) const {
  world_transform = input_transform_;
}

void PhysicsSystem::MotionState::setWorldTransform(
    const btTransform& interpolated_transform) {
  // Ignore the input transform because Bullet has interpolated it - we want the
  // raw world transform instead, else we will diverge when pushing data into
  // the simulation.
  physics_system_->UpdateLullabyTransform(entity_);
}

void PhysicsSystem::MotionState::SetKinematicTransform(
    const btTransform& world_transform) {
  input_transform_ = world_transform;
}

}  // namespace lull
