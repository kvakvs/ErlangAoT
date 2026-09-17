-module(literals).
value() -> {'атом', "line
?NOT_A_MACRO, -endif.", $\377, $\^?, $\x41, ~s"a\nb"suffix,
    ~S{?MACRO}, ~b/one\ttwo/, ~"default\n", ~Other[raw\n],
    """
      -define(NOT_A_DIRECTIVE, 1).
      ?NO_EXPANSION
      """,
    ~s"""
      decoded\ntext
      """,
    """"
      embedded """ quotes
      """"}.
