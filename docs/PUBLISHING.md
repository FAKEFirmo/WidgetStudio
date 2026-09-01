# Publishing and trusted signing

This guide separates repository publication from signing credentials. The repository contains only variable names and an inactive workflow path; it contains no account identifiers, API tokens, certificates, or private keys.

## What CI produces

`.github/workflows/windows-ci.yml` runs on GitHub-hosted Windows agents and:

1. builds Debug with MSVC, CMake, and Ninja;
2. runs Debug tests through CTest;
3. builds Release and runs Release tests;
4. creates and validates `WidgetStudio-portable.zip` outside the checkout;
5. uploads that ZIP as an unsigned GitHub Actions artifact;
6. optionally submits that exact artifact ID to SignPath and verifies the returned Authenticode signature.

External actions are pinned to full release commit SHAs. Review and deliberately update those pins when adopting a newer upstream release.

Ordinary pushes and pull requests never request signing. The signing job runs only when all of these are true: the workflow is manually dispatched, `sign_release` is selected, the chosen ref is a `v*` tag, and the repository variable `SIGNPATH_ENABLED` is exactly `true`.

## Manual GitHub publication steps

1. Create the repository yourself on GitHub as a **public** repository. Do not add generated starter files because this repository already has its history and license.
2. Use `main` as the default branch. Add the remote and push from your authenticated Git client; Codex does not need your token or password.
3. Replace `GITHUB_MAINTAINER` and `SIGNPATH_APPROVER` in `docs/CODE_SIGNING_POLICY.md` with real public names or team links. Commit and push that change.
4. Enable Actions. Keep workflow permissions read-only unless a later release workflow genuinely needs more.
5. In **Settings > Security > Private vulnerability reporting**, enable private reports so `SECURITY.md` has a working disclosure path.
6. Configure a branch ruleset for `main`: require pull requests and at least one approval when more than one maintainer is available, dismiss stale approvals, block force pushes and deletions, and protect workflow/build-script changes with real CODEOWNERS entries.
7. Confirm every maintainer uses multi-factor authentication.
8. Let the Windows workflow complete and download `WidgetStudio-portable.zip`. Test it, then publish an initial clearly labeled **unsigned** release candidate. SignPath Foundation requires the project to be public, documented, maintained, open source, and already released in the form that will be signed.

Do not add the SignPath secret or enable signing before the Foundation project exists.

## Manual SignPath Foundation steps

1. Review the [SignPath Foundation conditions](https://signpath.org/terms.html), enable MFA for every SignPath participant, then submit the [open-source application](https://signpath.org/apply.html) using the public repository and unsigned Release page.
2. After acceptance, open the assigned SignPath organization and create or confirm the WidgetStudio project. Set its Repository URL to the exact public GitHub repository URL.
3. In the organization, add the predefined trusted build system **GitHub.com**. Link it to the WidgetStudio project.
4. Install the SignPath GitHub App and grant it access only to the WidgetStudio repository. This enables GitHub origin verification.
5. Upload a CI-produced `WidgetStudio-portable.zip` as the sample for the artifact configuration. Its root is a ZIP containing `WidgetStudio/WidgetStudio.exe`. Configure Authenticode signing for that executable only.
6. Add file-metadata restrictions that require Product Name `WidgetStudio` and one consistent Product Version. The current CMake and Windows version resources both use `1.0.0`; update both together for a future release.
7. Create or select the Release signing policy backed by the SignPath Foundation certificate. Require trusted-build-system and origin verification and retain manual approval for every request. Assign the named Approver role.
8. Create an API token for a SignPath user that has only the submitter permission needed by this project and policy. Store it in GitHub as the Actions secret `SIGNPATH_API_TOKEN`; never place it in a file or paste it into an issue.
9. Add these GitHub **repository variables** using the exact values shown by SignPath:
   - `SIGNPATH_ORGANIZATION_ID`
   - `SIGNPATH_PROJECT_SLUG`
   - `SIGNPATH_SIGNING_POLICY_SLUG`
   - `SIGNPATH_ARTIFACT_CONFIGURATION_SLUG`
10. Add `SIGNPATH_ENABLED` with value `true` only after the other four variables and the secret are present and the role placeholders are resolved.

## Producing the signed acceptance candidate

1. Commit the intended release, wait for normal CI to pass, and create a version tag such as `v1.0.0` on that exact commit.
2. In **Actions > Windows CI and portable release > Run workflow**, select that tag and enable `sign_release`.
3. Review and manually approve the pending request in SignPath. The workflow will fail if the returned `WidgetStudio.exe` does not have a valid Authenticode signature.
4. Download the signed `WidgetStudio-portable.zip` workflow artifact. Record the SHA-256 digest from the workflow summary.
5. On the target Windows 11 system, extract and validate this exact ZIP without rebuilding it. Complete the GUI, desktop integration, media, startup-shortcut, Explorer restart, wallpaper, idle-resource, and mixed-DPI checks listed in `docs/ACCEPTANCE.md`.
6. Attach that same signed ZIP to the GitHub Release and include a **Code signing policy** link to `docs/CODE_SIGNING_POLICY.md` on the Release page.

If the signed archive fails final acceptance, fix the source and repeat with a new tag and signing request. Never replace a published tag or silently swap a release asset.
