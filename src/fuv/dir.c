#include "fuv.h"

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_dir_print, "dir");

static FklVMudFinalizeResult fuv_dir_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FuvValueDir *dir = FUV_DIR(ud);
    if (dir->req == NULL) {
        cleanUpDir(dir->dir, FUV_DIR_CLEANUP_ALL);
        dir->dir = NULL;
    }
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const FuvDirMt = {
	.name = "dir",
    .size = sizeof(FuvValueDir),
    .prin1 = fuv_dir_print,
    .princ = fuv_dir_print,
    .finalize = fuv_dir_finalize,
};

int isFuvDir(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &FuvDirMt;
}

FklVMvalue *
createFuvDir(FklVM *vm, FuvValueDll *dll, uv_fs_t *req, size_t nentries) {
    FklVMvalueType *tp = dll->DirType;
    FklVMvalue *v = fklCreateVMvalueUd(vm, tp);

    FuvValueDir *dir_ud = FUV_DIR(v);
    dir_ud->dir = req->ptr;
    req->ptr = NULL;
    uv_dir_t *dir = dir_ud->dir;
    dir->nentries = nentries;
    if (nentries) {
        dir->dirents = (uv_dirent_t *)fklZcalloc(nentries, sizeof(uv_dirent_t));
        FKL_ASSERT(dir->dirents);
    } else
        dir->dirents = NULL;
    return v;
}

FklVMvalueType *createFuvDirType(FklVM *vm, FklVMvalue *dll) {
    return fklCreateVMvalueType(vm, dll, &FuvDirMt, &FuvDirMt);
}

int isFuvDirUsing(FklVMvalue *dir) { return FUV_DIR(dir)->req != NULL; }

FklVMvalue *refFuvDir(FklVMvalue *dir_obj, FklVMvalue *req_obj) {
    FUV_DIR(dir_obj)->req = req_obj;
    return dir_obj;
}
