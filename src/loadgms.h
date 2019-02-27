#ifndef LOADGMS_H
#define LOADGMS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RETURN_OK = EXIT_SUCCESS,
    RETURN_ERROR = EXIT_FAILURE
} RETURN;

extern
RETURN loadGMS(
   struct gmoRec** gmo,
   struct gevRec** gev,
   const char* gmsfile
);


extern
void freeGMS(
   struct gmoRec** gmo,
   struct gevRec** gev
);

#ifdef __cplusplus
}
#endif

#endif
