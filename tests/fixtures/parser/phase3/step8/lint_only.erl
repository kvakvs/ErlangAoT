-module(lint_only).
f(#{a => X}, #missing{_ = X}) -> #missing{a = X}.
