// Host tests exercise callbacks without modifying machine code.
int DobbyHook(void *address, void *replacement, void **original) {
    (void)address; (void)replacement; (void)original; return -1;
}
int DobbyDestroy(void *address) { (void)address; return 0; }
