# Security Policy

## Reporting a Vulnerability
If you find a security vulnerability in our project, please report it so we can verify it.

For critical (ie.- RCE) issues you can email the main contributors directly or contact them via Discord.

Other lower impact vulnerabilities are probably fine reported as a public issue instead. vgmstream is mainly for local audio, so DoS or crashes are undesirable but not huge.

Theoretical issues (like undefined behavior or rare integer overflows) are best ignored or collectively reported for the time being, unless you have real use cases (that you have actually tested). They take a lot of effort to review and fix for minuscule benefit. Similarly please don't make tons of separate reports/patches at once.

Note that LLM-generated reports or patches with no clear human participation behind may be closed without warning. Please don't use the project as a testing ground or for credit farming.

## Supported Versions
vgmstream uses a rolling release model, so only the latest (HEAD) releases are supported.

Please only report issues against latest commits.

Tagged releases are published at semi-regular intervals based on the latest commits, and may be used as a reference, but please make sure to test against HEAD.
