    .include "init.s"
    .include "commandLoop.s"
    .include "logicCommands.s"
    .include "codeWindow.s"
    .export _commandLoop
    .export _initAsm
    .export _loadAndIncWinCode
    .export _afterLogicCommand
    .export _incCodeBy
    .export codeWindow

    .ifdef DEBUG
    .export _logDebugVal1
    .export _logDebugVal2
    .endif