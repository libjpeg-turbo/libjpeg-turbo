# Security Policy

## Supported Versions

Fixes for security vulnerabilities are proactively applied to any applicable
branch/release series that is in the
[Next-Gen, Active, Maintenance, or Extended support category](https://libjpeg-turbo.org/DeveloperInfo/Versioning).

## What Is a Security Vulnerability?

A security vulnerability is a bug in an official, supported release of
libjpeg-turbo whereby an otherwise well-behaved calling program can trigger a
potentially exploitable failure (such as a buffer overrun, an uninitialized
read, or undefined behavior) in one of the libraries by passing malformed image
data to a public API function.

- If the calling program itself is malformed and could not work properly with
  any image, then its inevitable failure is not a security vulnerability.  Such
  issues should be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md),
  and they will be investigated as opportunities for API hardening.

- If the issue affects only
  [Alpha/Evolving code](https://libjpeg-turbo.org/DeveloperInfo/Versioning) or
  has otherwise not officially been released, then it is not (yet) a security
  vulnerability.  Such issues should be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md).

- If the issue affects only an
  [EOL](https://libjpeg-turbo.org/DeveloperInfo/Versioning) branch/release
  series, then it is not a security vulnerability.  (Per above, fixes for
  security vulnerabilities are not proactively applied to EOL branches/release
  series.)  Such issues can be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md),
  but the suggested remedy will likely be to upgrade to a supported release.

## Reporting a Security Vulnerability

Vulnerabilities can be reported in one of the following ways:

- [E-mail the project admin](https://libjpeg-turbo.org/About/Contact).  You can
  optionally encrypt the e-mail using the provided public GPG key.

- [Beta and Post-Beta code](https://libjpeg-turbo.org/DeveloperInfo/Versioning)
  is not expected to be free of bugs, so vulnerabilities that affect only
  that code (for example, vulnerabilities introduced by a new feature that is
  not present in a Stable release series) can optionally be reported using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md).

## AI and False Positives

We do not specifically forbid the use of AI when analyzing libjpeg-turbo for
security vulnerabilities.  However, unfortunately AI-based security tools have
led to a flood of false positive reports against our project, and fielding
those reports has strained the project's limited resources.  As of this
writing, AI-based tools have yet to identify a single security vulnerability in
libjpeg-turbo.  Thus, we require security researchers who use AI to follow
these rules:

- Sanity check the results extremely carefully.  AI is notorious for generating
  reproducers that blatantly abuse the libjpeg and TurboJPEG APIs, such as by
  failing to allocate enough buffer space to hold the output image.  Per above,
  if a program can never generate a valid output image regardless of input,
  then its failure is not a security vulnerability.  At worst, it is an
  opportunity for API hardening.

- In the security report, always indicate whether and where AI was used.  If we
  know that the reproducer is AI-generated, then we can look for known AI
  hallucinations rather than waste time trying to reproduce the issue first.

- Never claim that an issue is a security vulnerability unless it meets the
  definition above, and always report the issue using a
  [GitHub bug report](https://github.com/libjpeg-turbo/libjpeg-turbo/issues/new?template=bug-report.md)
  if it does not meet that definition.  (This ensures that known false
  positives will be searchable in our issue tracker.)

- Security reports must originate from a human.  Automatic reporting from a
  bot/agent account is strictly forbidden and will result in the account being
  banned from our project.

Security reports that fail to adhere to these rules will be treated as spam.
