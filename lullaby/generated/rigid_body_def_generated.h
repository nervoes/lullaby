// Autogenerated code.  Do not edit.
#ifndef LULLABY_RIGID_BODY_DEF_GENERATED_H_
#define LULLABY_RIGID_BODY_DEF_GENERATED_H_

#include <type_traits>
#include <memory>
#include "flatbuffers/rigid_body_def_generated.h"
#include "lullaby/util/color.h"
#include "lullaby/util/common_types.h"
#include "lullaby/util/math.h"
#include "lullaby/util/optional.h"
#include "lullaby/util/typeid.h"
#include "physics_shapes_generated.h"

namespace lull {
class RigidBodyDefT;
class RigidBodyDefT {
 public:
  using FlatBufferType = RigidBodyDef;

  RigidBodyDefT() {}

  lull::RigidBodyType type = lull::RigidBodyType_Dynamic;
  std::vector<lull::PhysicsShapePartT> shapes;
  float mass = 0.0f;
  float friction = 0.5f;
  float restitution = 0.0f;
  bool enable_on_create = 1;
  lull::Optional<mathfu::vec3> linear_velocity;
  lull::Optional<mathfu::vec3> angular_velocity;
  lull::Optional<mathfu::vec3> center_of_mass_translation;

  template <typename Archive>
  void SerializeFlatbuffer(Archive archive);
};

template <typename Archive>
void RigidBodyDefT::SerializeFlatbuffer(Archive archive) {
  archive.VectorOfTables(&shapes, 6);
  int32_t _type = static_cast<int32_t>(type);
  archive.Scalar(&_type, 4, lull::RigidBodyType_Dynamic);
  type = static_cast<lull::RigidBodyType>(_type);
  archive.Scalar(&mass, 8, 0.0f);
  archive.Scalar(&friction, 10, 0.5f);
  archive.Scalar(&restitution, 12, 0.0f);
  archive.Scalar(&enable_on_create, 14, 1);
  archive.NativeStruct(&linear_velocity, 16);
  archive.NativeStruct(&angular_velocity, 18);
  archive.NativeStruct(&center_of_mass_translation, 20);
}

}  // namespace lull

LULLABY_SETUP_TYPEID(lull::RigidBodyDefT);

#endif  // LULLABY_RIGID_BODY_DEF_GENERATED_H_

