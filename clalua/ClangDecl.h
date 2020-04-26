#pragma once
#include <memory>
#include <string_view>
#include <vector>

namespace clalua
{

struct Decl
{
    virtual ~Decl()
    {
    }
};

struct TypeReference
{
    std::shared_ptr<Decl> decl;
    bool isConst = false;
};

struct Primitive : public Decl
{
};

struct Void : public Primitive
{
  private:
    Void()
    {
    }

  public:
    static std::shared_ptr<Void> instance()
    {
        static std::shared_ptr<Void> s_instance = std::shared_ptr<Void>(new Void());
        return s_instance;
    }
};

struct Bool : public Primitive
{
};

struct Int8 : public Primitive
{
};

struct Int16 : public Primitive
{
};

struct Int32 : public Primitive
{
};

struct Int64 : public Primitive
{
};

struct UInt8 : public Primitive
{
};

struct UInt16 : public Primitive
{
};

struct UInt32 : public Primitive
{
};

struct UInt64 : public Primitive
{
};

struct Float : public Primitive
{
};

struct Double : public Primitive
{
};

struct LongDouble : public Primitive
{
};

struct Pointer : public Decl
{
    std::shared_ptr<Decl> pointee;

    Pointer(const std::shared_ptr<Decl> &decl, bool isConst = false)
    {
        pointee = decl;
    }
};

struct Reference : public Decl
{
    std::shared_ptr<Decl> pointee;

    Reference(const std::shared_ptr<Decl> &decl, bool isConst = false)
    {
        pointee = decl;
    }
};

struct Array : public Decl
{
    std::shared_ptr<Decl> pointee;
    size_t size;

    Array(const std::shared_ptr<Decl> &decl, size_t size) : size(size)
    {
        pointee = decl;
    }
};

struct UserDecl : public Decl
{
    uint32_t hash;
    std::string path;
    uint32_t line;
    std::string name;

    UserDecl(uint32_t hash, const std::string_view &path, const uint32_t line, const std::string_view &name)
        : hash(hash), path(path), line(line), name(name)
    {
    }
};

struct Typedef : public UserDecl
{
    using UserDecl::UserDecl;
    TypeReference ref;

    static std::shared_ptr<Typedef> create(uint32_t hash, const std::string_view &path, const uint32_t line,
                                           const std::string_view &name)
    {
        return std::make_shared<Typedef>(hash, path, line, name);
    }
};

struct FunctionParam
{
    std::string name;
    TypeReference ref;
};

struct FunctionDecl : public UserDecl
{
    using UserDecl::UserDecl;
    TypeReference returnType;
    std::vector<FunctionParam> params;
    bool hasBody = false;
    bool dllExport = false;
    bool isVariadic = false;

    static std::shared_ptr<FunctionDecl> create(uint32_t hash, const std::string_view &path, const uint32_t line,
                                                const std::string_view &name)
    {
        return std::make_shared<FunctionDecl>(hash, path, line, name);
    }
};

struct EnumValue
{
    std::string name;
    uint32_t value;
};

struct EnumDecl : public UserDecl
{
    using UserDecl::UserDecl;
    std::vector<EnumValue> values;

    static std::shared_ptr<EnumDecl> create(uint32_t hash, const std::string_view &path, const uint32_t line,
                                            const std::string_view &name)
    {
        return std::make_shared<EnumDecl>(hash, path, line, name);
    }
};

struct Namespace : public UserDecl
{
    using UserDecl::UserDecl;

    static std::shared_ptr<Namespace> create(uint32_t hash, const std::string_view &path, const uint32_t line,
                                            const std::string_view &name)
    {
        return std::make_shared<Namespace>(hash, path, line, name);
    }
};

struct StructField
{
    uint32_t offset;
    std::string name;
    TypeReference ref;
};

struct StructDecl : public Namespace
{
    using Namespace::Namespace;

    bool isUnion = false;
    bool isForwardDecl = false;
    std::shared_ptr<StructDecl> definition;
    std::vector<StructField> fields;

    static std::shared_ptr<StructDecl> create(uint32_t hash, const std::string_view &path, const uint32_t line,
                                              const std::string_view &name)
    {
        return std::make_shared<StructDecl>(hash, path, line, name);
    }
};

} // namespace clalua
