#ifndef FALLOUT_SFALL_OPCODES_H_
#define FALLOUT_SFALL_OPCODES_H_

namespace fallout {

class ScriptHookCall;

ScriptHookCall* hookOpcodeGetCurrentCall(const char* opcodeName);

void sfallOpcodesInit();
void sfallOpcodesExit();

} // namespace fallout

#endif /* FALLOUT_SFALL_OPCODES_H_ */
