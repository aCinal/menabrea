#define FORMAT_LOG(fmt)  "params_parser.cc: " fmt
#include <logging.hh>
#include <menabrea/test/params_parser.hh>
#include <menabrea/exception.h>
#include <string.h>
#include <stdlib.h>

namespace ParamsParser {

static int ParseStructField(const StructField & field, const char * value, void * buf);

int Parse(char * str, void * buf, StructLayout && layout) {

    /* Split str into key=value tokens */
    char * saveptrToken = nullptr;
    char * token = strtok_r(str, " ", &saveptrToken);
    while (token != nullptr) {

        char * savePtrKeyValue = nullptr;
        /* Split each token at the equal sign into (key, value) pair */
        char * key = strtok_r(token, "=", &savePtrKeyValue);
        char * value = strtok_r(nullptr, "=", &savePtrKeyValue);
        if (value == nullptr) {

            LOG_WARNING("Failed to parse token '%s'", token);
            return -1;
        }

        /* Look up the key in the layout specification */
        if (layout.find(key) == layout.end()) {

            LOG_WARNING("Unknown key '%s'", key);
            return -1;
        }

        /* Parse the value into the corresponding field */
        StructField field = layout.at(key);
        if (ParseStructField(field, value, buf)) {

            LOG_WARNING("Failed to parse token %s=%s (value type: %d, value size: %d)", \
                key, value, static_cast<int>(field.Type), field.Size);
            return -1;
        }

        /* Remove the key from the map */
        layout.erase(key);

        token = strtok_r(nullptr, " ", &saveptrToken);
    }

    if (not layout.empty()) {

        LOG_WARNING("Not all required arguments provided! %ld parameter(s) missing:", \
            layout.size());
        /* Print out the missing parameters */
        for (auto & [key, value] : layout) {

            LOG_WARNING("    %s (type=%d, size=%d)", key.c_str(), static_cast<int>(value.Type), value.Size);
        }
        return -1;
    }

    return 0;
}

static int ParseStructField(const StructField & field, const char * value, void * buf) {

    void * addr = (u8 *) buf + field.Offset;
    char * endptr = nullptr;

    switch (field.Type) {
    case FieldType::U64:
        AssertTrue(field.Size == sizeof(u64));
        *static_cast<u64 *>(addr) = static_cast<u64>(strtoull(value, &endptr, 0));
        break;

    case FieldType::U32:
        AssertTrue(field.Size == sizeof(u32));
        *static_cast<u32 *>(addr) = static_cast<u32>(strtoull(value, &endptr, 0));
        break;

    case FieldType::U16:
        AssertTrue(field.Size == sizeof(u16));
        *static_cast<u16 *>(addr) = static_cast<u16>(strtoull(value, &endptr, 0));
        break;

    case FieldType::U8:
        AssertTrue(field.Size == sizeof(u8));
        *static_cast<u8 *>(addr) = static_cast<u8>(strtoull(value, &endptr, 0));
        break;

    case FieldType::I64:
        AssertTrue(field.Size == sizeof(i64));
        *static_cast<i64 *>(addr) = static_cast<i64>(strtoull(value, &endptr, 0));
        break;

    case FieldType::I32:
        AssertTrue(field.Size == sizeof(i32));
        *static_cast<i32 *>(addr) = static_cast<i32>(strtoull(value, &endptr, 0));
        break;

    case FieldType::I16:
        AssertTrue(field.Size == sizeof(i16));
        *static_cast<i16 *>(addr) = static_cast<i16>(strtoull(value, &endptr, 0));
        break;

    case FieldType::I8:
        AssertTrue(field.Size == sizeof(i8));
        *static_cast<i8 *>(addr) = static_cast<i8>(strtoull(value, &endptr, 0));
        break;

    case FieldType::Boolean:
        AssertTrue(field.Size == sizeof(bool));
        if (0 == strcmp(value, "true")) {

            *static_cast<bool *>(addr) = true;

        } else if (0 == strcmp(value, "false")) {

            *static_cast<bool *>(addr) = false;

        } else {

            LOG_WARNING("Unexpected value of a boolean field: '%s' (expected true/false)", \
                value);
            return -1;
        }
        return 0;

    case FieldType::String:
        /* Check the size (note that equality also fails the condition as there would be
         * no room for the null terminator) */
        if (strlen(value) >= field.Size) {

            LOG_WARNING("String argument too large (string length: %ld, field size: %d): '%s'", \
                strlen(value), field.Size, value);
            return -1;
        }
        /* Safely copy the string now that we have checked the field size */
        (void) strcpy(static_cast<char *>(addr), value);
        return 0;

    default:
        RaiseException(EExceptionFatality_Fatal, \
            "Unknown field type %d of size %d at offset %d in test case args layout specification", \
            static_cast<int>(field.Type), field.Size, field.Offset);
        return -1;
    }

    return -(endptr - value != static_cast<long>(strlen(value)));
}

}
