    .include "init.s"
    .include "commandLoop.s"
    .include "logicCommands.s"
    .include "codeWindow.s"
    .include "global.s"
    .include "graphics.s"
    .include "picture.s"
    .export _executeLogic
    .export _b7InitAsm
    .export _loadAndIncWinCode
    .export _incCodeBy
    .export codeWindow
    .export _codeBank
    .export _logDebugVal1
    .export _logDebugVal2
    .export _b7DisableAndWaitForVsync
    .export _b7ClearBackground
    .export _b11Drawline
    .export _bresenham_x1
    .export _toDraw
    .export _drawWhere
    .export _pixelCounter
.export _bresenham_x2
.export _bresenham_y1
.export _bresenham_y2