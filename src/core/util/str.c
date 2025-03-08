#include <core/util/str.h>

#include <ctype.h>
#include <string.h>

char*  trim_trailing_whitespace(char *str) {
    if (!str || *str == '\0')
        return str;
    
    size_t len = strlen(str);
    char* end = str + len - 1;
    
    while (end >= str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    
    return str;
}
