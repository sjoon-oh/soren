/* github.com/sjoon-oh/soren
 * @author: Sukjoon Oh, sjoon@kaist.ac.kr
 * 
 * Project SOREN
 */

#include "commons.hh"

const bool soren::isSlotValid(Slot& arg_tar, uint32_t arg_can) { return arg_tar.canary == arg_can; }