/*
fn parse_expr_list(vars: &FTerm) -> Vec<Expr> {
  if !vars.is_list() {
    panic!("List of something is expected, found {}", vars)
  }
  let mut result = Vec::<Expr>::new();
  for v in vars.get_list_vec() {
    result.push(_parse_expr_2(&v))
  }
  result
}


fn parse_expr(expr: &FTerm) -> Expr {
  if expr.is_int() {
    return Expr::Int64(expr.get_i64())
  } else if expr.is_atom() {
    return Expr::Atom(expr.get_atom_text())
  } else if expr.is_list() {
    return Expr::MultipleExprs(parse_expr_list(expr))
  }
  return _parse_expr_2(expr)
}


fn _parse_expr_2(expr: &FTerm) -> Expr {
  // So val is a tuple, parse it as a k_* tuple or something
  let vvec = expr.get_tuple_vec();
  match vvec[0].get_atom_text().as_ref() {
    "k_var" => {
      // {k_var, anno, name}
      assert!(vvec[0].is_atom_of("k_var"));
      match &vvec[2] {
        FTerm::Atom(s) => Expr::Variable(s.to_string()),
        FTerm::Int64(i) => Expr::Variable(i.to_string()),
        _ => panic!("Don't know how to parse val {}", vvec[2]),
      }
    },
    "k_bif" => Expr::Bif(Box::new(parse_kcall(vvec))),
    "k_atom" => { // {k_atom, anno, val}
      Expr::Atom(vvec[2].get_atom_text())
    },
    "k_int" => { // {k_int, anno, val}
      Expr::Int64(vvec[2].get_i64())
    },
    "k_call" => Expr::Call(Box::new(parse_kcall(vvec))),
    "k_literal" => // {k_literal, anno, val}
      Expr::Literal {
        anno: vvec[1].clone(),
        val: vvec[2].clone()
      },
    // TODO: k_put
    // TODO: k_test
    // TODO: k_enter
    // TODO: k_match
    // TODO: k_guard_match
    // TODO: k_try, k_try_enter
    // TODO: k_catch
    // TODO: k_receive, k_receive_accept, k_receive_next
    // TODO: k_break
    // TODO: k_guard_break
    // TODO: k_return

    other => panic!("_parse_expr_2 doesn't know how to handle {} in {:?}",
                    vvec[0], vvec)
  }
}
*/