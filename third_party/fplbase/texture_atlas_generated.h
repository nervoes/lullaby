// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_TEXTUREATLAS_ATLASDEF_H_
#define FLATBUFFERS_GENERATED_TEXTUREATLAS_ATLASDEF_H_

#include "flatbuffers/flatbuffers.h"

#include "common_generated.h"

namespace atlasdef {

struct TextureAtlasEntry;

struct TextureAtlas;

struct TextureAtlasEntry FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_LOCATION = 6,
    VT_SIZE = 8
  };
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  const fplbase::Vec2 *location() const {
    return GetStruct<const fplbase::Vec2 *>(VT_LOCATION);
  }
  const fplbase::Vec2 *size() const {
    return GetStruct<const fplbase::Vec2 *>(VT_SIZE);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyField<fplbase::Vec2>(verifier, VT_LOCATION) &&
           VerifyField<fplbase::Vec2>(verifier, VT_SIZE) &&
           verifier.EndTable();
  }
};

struct TextureAtlasEntryBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(TextureAtlasEntry::VT_NAME, name);
  }
  void add_location(const fplbase::Vec2 *location) {
    fbb_.AddStruct(TextureAtlasEntry::VT_LOCATION, location);
  }
  void add_size(const fplbase::Vec2 *size) {
    fbb_.AddStruct(TextureAtlasEntry::VT_SIZE, size);
  }
  TextureAtlasEntryBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  TextureAtlasEntryBuilder &operator=(const TextureAtlasEntryBuilder &);
  flatbuffers::Offset<TextureAtlasEntry> Finish() {
    const auto end = fbb_.EndTable(start_, 3);
    auto o = flatbuffers::Offset<TextureAtlasEntry>(end);
    return o;
  }
};

inline flatbuffers::Offset<TextureAtlasEntry> CreateTextureAtlasEntry(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    const fplbase::Vec2 *location = 0,
    const fplbase::Vec2 *size = 0) {
  TextureAtlasEntryBuilder builder_(_fbb);
  builder_.add_size(size);
  builder_.add_location(location);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<TextureAtlasEntry> CreateTextureAtlasEntryDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *name = nullptr,
    const fplbase::Vec2 *location = 0,
    const fplbase::Vec2 *size = 0) {
  return atlasdef::CreateTextureAtlasEntry(
      _fbb,
      name ? _fbb.CreateString(name) : 0,
      location,
      size);
}

struct TextureAtlas FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_TEXTURE_FILENAME = 4,
    VT_ENTRIES = 6
  };
  const flatbuffers::String *texture_filename() const {
    return GetPointer<const flatbuffers::String *>(VT_TEXTURE_FILENAME);
  }
  const flatbuffers::Vector<flatbuffers::Offset<TextureAtlasEntry>> *entries() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<TextureAtlasEntry>> *>(VT_ENTRIES);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_TEXTURE_FILENAME) &&
           verifier.Verify(texture_filename()) &&
           VerifyOffset(verifier, VT_ENTRIES) &&
           verifier.Verify(entries()) &&
           verifier.VerifyVectorOfTables(entries()) &&
           verifier.EndTable();
  }
};

struct TextureAtlasBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_texture_filename(flatbuffers::Offset<flatbuffers::String> texture_filename) {
    fbb_.AddOffset(TextureAtlas::VT_TEXTURE_FILENAME, texture_filename);
  }
  void add_entries(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<TextureAtlasEntry>>> entries) {
    fbb_.AddOffset(TextureAtlas::VT_ENTRIES, entries);
  }
  TextureAtlasBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  TextureAtlasBuilder &operator=(const TextureAtlasBuilder &);
  flatbuffers::Offset<TextureAtlas> Finish() {
    const auto end = fbb_.EndTable(start_, 2);
    auto o = flatbuffers::Offset<TextureAtlas>(end);
    return o;
  }
};

inline flatbuffers::Offset<TextureAtlas> CreateTextureAtlas(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> texture_filename = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<TextureAtlasEntry>>> entries = 0) {
  TextureAtlasBuilder builder_(_fbb);
  builder_.add_entries(entries);
  builder_.add_texture_filename(texture_filename);
  return builder_.Finish();
}

inline flatbuffers::Offset<TextureAtlas> CreateTextureAtlasDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *texture_filename = nullptr,
    const std::vector<flatbuffers::Offset<TextureAtlasEntry>> *entries = nullptr) {
  return atlasdef::CreateTextureAtlas(
      _fbb,
      texture_filename ? _fbb.CreateString(texture_filename) : 0,
      entries ? _fbb.CreateVector<flatbuffers::Offset<TextureAtlasEntry>>(*entries) : 0);
}

inline const atlasdef::TextureAtlas *GetTextureAtlas(const void *buf) {
  return flatbuffers::GetRoot<atlasdef::TextureAtlas>(buf);
}

inline bool VerifyTextureAtlasBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<atlasdef::TextureAtlas>(nullptr);
}

inline void FinishTextureAtlasBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<atlasdef::TextureAtlas> root) {
  fbb.Finish(root);
}

}  // namespace atlasdef

#endif  // FLATBUFFERS_GENERATED_TEXTUREATLAS_ATLASDEF_H_