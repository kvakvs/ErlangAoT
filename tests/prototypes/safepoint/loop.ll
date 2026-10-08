; Step-24 decision prototype: one comprehension loop with a safepoint at its head, written the way lower_frames
; leaves a body. WORD is the target word (run.py substitutes i64 or i32). `make` and `next` stand in for the
; allocating construct and inspection services; real bodies end in musttail transfers instead of `ret`.
;
;   f(L) -> Y = make(L, L), loop over L: Acc = make(Y, Acc).
;
; Frame slots after the 4-word header: 0 = L (argument), 1 = cursor, 2 = accumulator, 3 = Y. All four are term
; slots (roots), so a collection at the safepoint rewrites them; Y is the SSA value that crosses the safepoint.

declare ptr @CLAUSE_frame_v1(ptr)
declare void @CLAUSE_safepoint_v1(ptr)
declare WORD @make(ptr, WORD, WORD)
declare WORD @next(ptr, WORD)

define WORD @f.body(ptr %context) {
frame:
  %header = call ptr @CLAUSE_frame_v1(ptr %context)
  %slots = getelementptr inbounds WORD, ptr %header, i32 4
  %cursor.slot = getelementptr inbounds WORD, ptr %slots, i32 1
  %acc.slot = getelementptr inbounds WORD, ptr %slots, i32 2
  %y.slot = getelementptr inbounds WORD, ptr %slots, i32 3
  %l = load WORD, ptr %slots
  %y = call WORD @make(ptr %context, WORD %l, WORD %l)
  ; Spill: Y is read after the safepoint, so it is stored in a term slot right after its definition.
  store WORD %y, ptr %y.slot
  store WORD %l, ptr %cursor.slot
  store WORD 59, ptr %acc.slot
  br label %head

head:
  ; May move every heap object and rewrite the term slots; the stack itself never moves here.
  call void @CLAUSE_safepoint_v1(ptr %context)
  br label %step

step:
  %cursor = load WORD, ptr %cursor.slot
  %rest = call WORD @next(ptr %context, WORD %cursor)
  %done = icmp eq WORD %rest, 0
  br i1 %done, label %exit, label %body

body:
  store WORD %rest, ptr %cursor.slot
  ; Reload after the safepoint: the SSA %y may name the old copy.
  %y.reload = load WORD, ptr %y.slot
  %acc = load WORD, ptr %acc.slot
  %cell = call WORD @make(ptr %context, WORD %y.reload, WORD %acc)
  store WORD %cell, ptr %acc.slot
  br label %head

exit:
  %result = load WORD, ptr %acc.slot
  ret WORD %result
}
