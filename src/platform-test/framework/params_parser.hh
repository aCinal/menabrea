
#ifndef PLATFOMR_TEST_FRAMEWORK_PARAMS_PARSER_HH
#define PLATFOMR_TEST_FRAMEWORK_PARAMS_PARSER_HH

#include <menabrea/common.h>
#include <map>
#include <string>

namespace ParamsParser {

enum class FieldType {
    Undefined,
    U64,
    U32,
    U16,
    U8,
    I64,
    I32,
    I16,
    I8,
    Boolean,
    String
};

struct StructField {
    /* Define a no-arguments constructor to allow the following syntax:
     * structLayout[key] = StructField(4, sizeof(u16), FieldType::U16); */
    StructField(void) : StructField(0, 0, FieldType::Undefined) {}
    /* Generic constructor */
    StructField(u32 offset, u32 size, FieldType type) : Offset(offset), Size(size), Type(type) {}
    /* Offset into the structure */
    u32 Offset;
    /* Size of the field */
    u32 Size;
    /* Type of the field */
    FieldType Type;
};

using StructLayout = std::map<std::string, StructField>;
int Parse(char * str, void * buf, StructLayout && layout);

}

#endif /* PLATFOMR_TEST_FRAMEWORK_PARAMS_PARSER_HH */
