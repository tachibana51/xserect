#include <cstddef>
#ifndef C_CODE_H
#define C_CODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RetData RetData;
struct RetData {
   unsigned char *data;
   size_t size;
   unsigned int w;
   unsigned int h;
};
RetData doGlabScreen();

#ifdef __cplusplus
}
#endif

#endif // C_CODE_H

