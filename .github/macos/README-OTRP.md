# OTRP macOS signing and notarization

The signing scripts in this directory are from simutrans/simutrans commit
[2ddb494099000f16ff94be9ba5e3709675feba79](https://github.com/simutrans/simutrans/commit/2ddb494099000f16ff94be9ba5e3709675feba79).
The OTRP workflow adapts the build and artifact layout. It builds on an
unprivileged runner, then checks the payload and signs and notarizes it on a
separate runner. The Developer ID is loaded only in the latter job.

A manual run on `OTRP-KUTAv6` produces a signed, notarized ARM64 ZIP as an
Actions artifact. In addition, a `v*` tag pointing
to a commit on `OTRP-KUTAv6` triggers a separate trusted workflow after the
tag's ordinary macOS build succeeds. That workflow checks that the tag still
points to the triggering commit and that the commit is in the branch's
history, builds that commit without credentials, and signs it in a separate
job. For eligible tags, the ordinary build does not upload an ad-hoc artifact;
the original artifact name is used by the notarization workflow instead.
PR builds, other tags and manual runs on other branches keep the existing
ad-hoc-signed app artifact. PR code is never executed in a signing job. This
does not publish a GitHub release.
GitHub offers manual dispatch only when the workflow also exists on the
repository's default branch. An ordinary push to `OTRP-KUTAv6` does not start
this macOS workflow.

The tag-signing workflow uses `workflow_run`, so it must exist on the default
branch, and the default branch must be `OTRP-KUTAv6` for the Environment
branch rule below to allow it to sign. The tagged commit must have a macOS
workflow named `OTRP build MacOS` at `.github/workflows/otrp-macos.yml` and
that workflow must complete successfully. The ordinary tag build and the
signed build appear as separate Actions runs; download the artifact from
`OTRP notarize macOS tag`. The tagged source is rebuilt in that workflow so
even a historical tag whose macOS workflow predates notarization can be
processed under these conditions. Such an older workflow may also upload an
ad-hoc artifact with the same name in its own run; that artifact is not notarized.

## GitHub setup

Before merging this workflow into `OTRP-KUTAv6`:

1. In the destination repository, open **Settings → Environments**, create
   `macos-signing`, and set **Deployment branches and tags → Selected branches
   and tags** to the single branch `OTRP-KUTAv6`. Protect that branch so that
   only trusted, reviewed changes reach it. Do not add a PR merge-ref rule.
   Environment features and protection rules depend on repository visibility
   and GitHub plan. A required reviewer can provide an additional human gate.
2. Under that environment's **Environment secrets**, add the following five
   values from `~/Downloads/otrp-certs/para-himeshi-otrp/`. The `.txt` files
   already contain the required values; P12 and P8 are already base64 encoded.
   Do not encode them again.
   - `MACOS_CERTIFICATE_P12`
   - `MACOS_CERTIFICATE_P12_PASSWORD`
   - `MACOS_NOTARY_API_KEY_P8`
   - `MACOS_NOTARY_API_KEY_ID`
   - `MACOS_NOTARY_API_ISSUER_ID`
3. Under **Environment variables**, add `MACOS_SIGNING_IDENTITY` and
   `MACOS_TEAM_ID` using the values in the supplied `README.txt`. The identity
   must exactly match the Developer ID Application certificate.

Alternatively, after creating the environment, register the secrets without
printing them with GitHub CLI. Replace `OWNER/REPO` with the actual destination
repository and confirm `gh auth status` first:

```sh
cert_dir="$HOME/Downloads/otrp-certs/para-himeshi-otrp"
for name in MACOS_CERTIFICATE_P12 MACOS_CERTIFICATE_P12_PASSWORD \
  MACOS_NOTARY_API_KEY_P8 MACOS_NOTARY_API_KEY_ID MACOS_NOTARY_API_ISSUER_ID; do
  gh secret set "$name" --repo OWNER/REPO --env macos-signing < "$cert_dir/$name.txt"
done
```

The manual run's signed artifact is `sim-mac-OTRP_OTRP-KUTAv6.app`: download it
from Actions and extract the ZIP inside it. The ZIP preserves the app's permissions
and stapled ticket. Test the downloaded app on a separate Mac before release.
GitHub Actions artifacts in a public repository are publicly downloadable.
For example, tag `v1.2.3` produces the original artifact name
`sim-mac-OTRPv1.2.3.app`, containing `sim-mac-OTRPv1.2.3.zip`, which contains
`sim-mac-OTRPv1.2.3.app`. No `v*` tag rule is
needed in the signing Environment: the trusted tag workflow runs on the
default branch rather than the tag ref.

The upstream scripts retry specific transient CloudKit failures when stapling
or validating. This adaptation does **not** implement upstream's encrypted
archive retention and resume workflows. If Apple's verdict arrives after the
job times out, the signed submission archive is gone and cannot be resumed;
check the submission ID in the logs before choosing to run the workflow again.
`MACOS_ARTIFACT_KEY` is therefore not used. A successful CI job requires an
accepted verdict, successful staple and verification of the extracted ZIP.
