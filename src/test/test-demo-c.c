/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

//
// C-based integrator test.
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../soren-c.h"

int main() {

    char* test_payload = "Literature adds to reality, it does not simply describe it. \
It enriches the necessary competencies that daily life requires and provides; and in \
this respect, it irrigates the deserts that our lives have already become.";

    char* target = test_payload;

    printf("SOREN C-WRAPPER DEMO\n");
    cwInitSoren(NULL);

    int32_t ts_idx;
    cwInitTs(1000);

    for (int i = 0; i < 10; i++) {
        for (int off = 0; off < strlen(test_payload); off += 17) {
            ts_idx = cwMarkTsBefore();
            cwPropose((uint8_t*)(target + off), 17, (uint8_t*)(target + off), 4, 0);
            cwMarkTsAfter(ts_idx);
        }
    }

    sleep(20);

    printf("Exporting statistics...\n");
    cwDumpTs();

    printf("Cleaning up SOREN...\n");

    cwCleanSoren();
    printf("SOREN C-WRAPPER END\n");

    return 0;
}