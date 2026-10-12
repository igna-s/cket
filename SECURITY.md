# Reporting a vulnerability

`|c>` is experimental. IR parsers, expression validation, synthesis budgets, and GCC integration are
part of its security surface. The README describes ASan/UBSan and fuzzing evidence; this is not a
formal audit.

Since the repository is private, use
[the security form within the repository](https://github.com/igna-s/cket/issues/new?template=security_report.yml),
which is accessible to collaborators. Include a minimal C program or QIR document, the reproduction
command, the versions used, and the observed impact. Omit credentials and sensitive data from
examples.

Only the current experimental version receives changes in this repository. There is no stable
version support policy or guaranteed response time yet.
