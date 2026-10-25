# Rule (g) of the include lint (ADR-0007 section 3; story tdc-014), run through cpplex.awk over each
# header under Public/Server/: every UCLASS declares NotBlueprintType and NotBlueprintable, every
# UINTERFACE declares meta=(CannotImplementInterfaceInBlueprint), and no UPROPERTY/UFUNCTION carries
# BlueprintCallable, BlueprintPure, BlueprintReadOnly, BlueprintReadWrite or BlueprintAssignable.
# The specifier list is read up to its balanced closing parenthesis, so lists that span lines and
# nested meta=(...) lists are handled. Only observations are printed; the gate decides.
function rule_file(path,   pos, abs, txt, name, open, endp, args, ln, i, n, banned, hits) {
  n = split("BlueprintCallable BlueprintPure BlueprintReadOnly BlueprintReadWrite BlueprintAssignable", banned, " ")
  pos = 1
  while (match(substr(CODE, pos), /(^|[^A-Za-z0-9_])(UCLASS|UINTERFACE|UPROPERTY|UFUNCTION)[ \t\n]*\(/)) {
    abs = pos + RSTART - 1
    txt = substr(CODE, abs, RLENGTH)
    open = abs + RLENGTH - 1
    sub(/^[^A-Za-z0-9_]/, "", txt)
    name = txt; sub(/[ \t\n]*\($/, "", name)
    ln = lex_line(open)
    endp = lex_balanced(CODE, open)
    if (endp == 0) {
      lint_find(path, ln, "g", name " macro has no balanced closing parenthesis")
      pos = open + 1
      continue
    }
    args = substr(CODE, open + 1, endp - open - 1)
    if (name == "UCLASS") {
      if (!lex_has_token(args, "NotBlueprintType") || !lex_has_token(args, "NotBlueprintable"))
        lint_find(path, ln, "g", "server UCLASS must declare NotBlueprintType, NotBlueprintable")
    } else if (name == "UINTERFACE") {
      if (!lex_has_token(args, "meta") || !lex_has_token(args, "CannotImplementInterfaceInBlueprint"))
        lint_find(path, ln, "g", "server UINTERFACE must declare meta=(CannotImplementInterfaceInBlueprint)")
    } else {
      hits = ""
      for (i = 1; i <= n; i++) if (lex_has_token(args, banned[i])) hits = hits (hits == "" ? "" : ", ") banned[i]
      if (hits != "") lint_find(path, ln, "g", "member of a server type carries " hits " (server types are never Blueprint-exposed)")
    }
    pos = endp + 1
  }
}
