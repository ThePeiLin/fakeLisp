#include "fuv.h"

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_dir_as_print, "dir");

static int fuv_dir_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(dir, FuvDirUd, ud);
    unrefFuvDir(*dir);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable FuvDirUdMetaTable = {
    .size = sizeof(FuvDirUd),
    .__as_prin1 = fuv_dir_as_print,
    .__as_princ = fuv_dir_as_print,
    .__finalizer = fuv_dir_finalizer,
};

int isFuvDir(FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &FuvDirUdMetaTable;
}

FklVMvalue *createFuvDir(FklVM *vm, FklVMvalue *rel, uv_fs_t *req,
                         size_t nentries) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FuvDirUdMetaTable, rel);
    FKL_DECL_VM_UD_DATA(dir_ud, FuvDirUd, v);
    FuvDir *fdir = (FuvDir *)calloc(1, sizeof(FuvDir));
    FKL_ASSERT(fdir);
    *dir_ud = fdir;
    fdir->dir = req->ptr;
    req->ptr = NULL;
    uv_dir_t *dir = fdir->dir;
    dir->nentries = nentries;
    if (nentries) {
        dir->dirents = (uv_dirent_t *)calloc(nentries, sizeof(uv_dirent_t));
        FKL_ASSERT(dir->dirents);
    } else
        dir->dirents = NULL;
    return v;
}

int isFuvDirUsing(FuvDir *dir) { return atomic_load(&dir->ref); }

FuvDir *refFuvDir(FuvDir *dir) {
    atomic_fetch_add(&dir->ref, 1);
    return dir;
}

void unrefFuvDir(FuvDir *dir) {
    if (atomic_load(&dir->ref))
        atomic_fetch_sub(&dir->ref, 1);
    else {
        uv_fs_t req;
        uv_dir_t *d = dir->dir;
        if (d) {
            if (d->dirents) {
                d->nentries = 0;
                free(d->dirents);
                d->dirents = NULL;
            }
            uv_fs_closedir(NULL, &req, d, NULL);
        }
        free(dir);
    }
}
