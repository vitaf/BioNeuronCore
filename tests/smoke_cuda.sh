#!/bin/bash
# smoke_cuda.sh — быстрая проверка native CUDA backend (Smoke test #11)
# Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
# Usage: bash smoke_cuda.sh [path_to_libbiocore_gpu_backend_cuda.so]

SO_PATH="${1:-./libbiocore_gpu_backend_cuda.so}"
PASS=0; FAIL=0

check() {
    local desc="$1"; shift
    if "$@"; then echo "  ✓ $desc"; PASS=$((PASS+1))
    else echo "  ✗ FAIL: $desc"; FAIL=$((FAIL+1)); fi
}

echo "=== CUDA Native Backend Smoke Test ==="

# 1. .so exists
check ".so file exists" test -f "$SO_PATH"

# 2. .so is valid ELF
check ".so is ELF shared object" file "$SO_PATH" | grep -q "shared object"

# 3. .so exports required symbols
for sym in gpu_backend_create gpu_backend_create_on_device gpu_backend_destroy \
           gpu_backend_load_bion gpu_backend_run_tick gpu_backend_bion_data \
           gpu_backend_device_count gpu_backend_get_device_info \
           gpu_backend_ibuf_offset gpu_backend_vm_offset gpu_backend_seg_size; do
    check "exports $sym" nm -D "$SO_PATH" | grep -q "T $sym"
done

# 4. dlopen + create + destroy (quick C test)
cat > /tmp/smoke_cuda_test.c << 'EOF'
#include <dlfcn.h>
#include <stdio.h>
int main() {
    void* h = dlopen("./libbiocore_gpu_backend_cuda.so", RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 1; }
    void* (*devcount)() = dlsym(h, "gpu_backend_device_count");
    if (!devcount) { printf("dlsym failed\n"); return 2; }
    int ndev = devcount();
    printf("CUDA devices: %d\n", ndev);
    if (ndev <= 0) { printf("no CUDA devices\n"); dlclose(h); return 3; }
    void* (*create)() = dlsym(h, "gpu_backend_create");
    void (*destroy)(void*) = dlsym(h, "gpu_backend_destroy");
    void* (*segsz)() = dlsym(h, "gpu_backend_seg_size");
    void* sim = create();
    if (!sim) { printf("create failed\n"); dlclose(h); return 4; }
    printf("sizeof(Segment) from GPU .so: %d\n", (int)(intptr_t)segsz());
    destroy(sim);
    dlclose(h);
    printf("CUDA SMOKE OK\n");
    return 0;
}
EOF
gcc -o /tmp/smoke_cuda_test /tmp/smoke_cuda_test.c -ldl 2>/dev/null
cd "$(dirname "$SO_PATH")" && /tmp/smoke_cuda_test 2>&1
check "dlopen + create + destroy" [ $? -eq 0 ]

echo "=== Result: $PASS passed, $FAIL failed ==="
rm -f /tmp/smoke_cuda_test /tmp/smoke_cuda_test.c
exit $FAIL
