#include <climits>
#include <cstdbool>
#include <cstdint>
#include <cstring>
#include <arcan_shmif.h>
#include "nfd.h"

void NFD_FreePathN(nfdnchar_t* filePath)
{
}

nfdresult_t NFD_Init(void)
{
    return NFD_OKAY;
}

nfdresult_t NFD_OpenDialogN(nfdnchar_t** outPath,
                                    const nfdnfilteritem_t* filterList,
                                    nfdfiltersize_t filterCount,
                                    const nfdnchar_t* defaultPath)
{
    struct arcan_shmif_cont* C = arcan_shmif_primary(SHMIF_INPUT);
    if (!C){
        return NFD_CANCEL;
    }
/* convert filterList into an immediate request,
 * with bchunk.extensions popuplated with filterList */
    arcan_event ev;
    memset(&ev, '\0', sizeof(struct arcan_event));
    ev.category = EVENT_EXTERNAL;
    ev.ext.kind = EVENT_EXTERNAL_BCHUNKSTATE;
    ev.ext.bchunk.input = 1;
    ev.ext.bchunk.stream = false;
    ev.ext.bchunk.extensions[0] = '*';
    ev.ext.bchunk.hint = 1;

    arcan_shmif_enqueue(C, &ev);

/* outPath */
    return NFD_OKAY;
}

nfdresult_t NFD_OpenDialogMultipleN(const nfdpathset_t** outPaths,
                                            const nfdnfilteritem_t* filterList,
                                            nfdfiltersize_t filterCount,
                                            const nfdnchar_t* defaultPath)
{
    return NFD_CANCEL;
}

nfdresult_t NFD_SaveDialogN(nfdnchar_t** outPath,
                                    const nfdnfilteritem_t* filterList,
                                    nfdfiltersize_t filterCount,
                                    const nfdnchar_t* defaultPath,
                                    const nfdnchar_t* defaultName)
{
    struct arcan_shmif_cont* C = arcan_shmif_primary(SHMIF_INPUT);
    if (!C){
        return NFD_CANCEL;
    }
    arcan_event ev;
    memset(&ev, '\0', sizeof(struct arcan_event));
    ev.category = EVENT_EXTERNAL;
    ev.ext.kind = EVENT_EXTERNAL_BCHUNKSTATE;
    ev.ext.bchunk.input = 0;
    ev.ext.bchunk.stream = false;
    ev.ext.bchunk.extensions[0] = '*';
    ev.ext.bchunk.hint = 1;

    arcan_shmif_enqueue(C, &ev);
    return NFD_CANCEL;
}

const char* NFD_GetError(void)
{
    return nullptr;
}

void NFD_ClearError(void)
{
}

void NFD_Quit(void) {
}

nfdresult_t NFD_PickFolderN(nfdnchar_t** outPath, const nfdnchar_t* defaultPath)
{
    return NFD_CANCEL;
}

nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t* pathSet, nfdpathsetsize_t* count)
{
    *count = 0;
    return NFD_CANCEL;
}

nfdresult_t NFD_PathSet_GetPathN(const nfdpathset_t* pathSet,
                                         nfdpathsetsize_t index,
                                         nfdnchar_t** outPath)
{
    return NFD_OKAY;
}

nfdresult_t NFD_PathSet_GetEnum(const nfdpathset_t* pathSet,
                                        nfdpathsetenum_t* outEnumerator)
{
    outEnumerator->ptr = const_cast<void*>(pathSet);
    return NFD_OKAY;
}

nfdresult_t NFD_PathSet_EnumNextN(nfdpathsetenum_t* enumerator, nfdnchar_t** outPath)
{
    *outPath = nullptr;
    return NFD_OKAY;
}

void NFD_PathSet_FreeEnum(nfdpathsetenum_t* enumerator)
{
}

void NFD_PathSet_Free(const nfdpathset_t* pathSet)
{
}
