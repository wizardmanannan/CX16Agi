.ifdef LOGIC_APP_REPLAY_TARGET

.export _logicAppReplayBoot
.export _logicAppReplayStep
.import _logicAppReplayBootBody
.import _logicAppReplayStepBody

.segment "CODE"

_logicAppReplayBoot:
    jsr _logicAppReplayBootBody
    rts

_logicAppReplayStep:
    jsr _logicAppReplayStepBody
    rts

.endif
