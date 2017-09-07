#include <scfile.h>

#define _STR_CNF    "test/config/scfile.ini"

static void _scfile(void * arg, FILE * cnf) {
    char txt[BUFSIZ];
    fgets(txt, sizeof txt, cnf);
    puts(txt);
}

//
// test config file update
//
void test_scfile(void) {
    // ��ʼע��һ��
    file_set(_STR_CNF, NULL, _scfile);

    // ��ʽˢ��
    for (;;) {
        file_update();
        // 1s һ����
        sh_msleep(1000);
    }
}
