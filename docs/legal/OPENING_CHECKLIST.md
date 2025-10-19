# Public Opening Checklist

- [ ] Remove/replace non-redistributable SDKs or binaries; keep headers and stubs.
- [ ] Add SPDX headers to all sources (`SPDX-License-Identifier: AGPL-3.0-or-later`).
- [ ] Include LICENSE, LICENSE_SUMMARY.md, PATENTS.md, NOTICE.
- [ ] Include docs/ModBatt_Product_License.pdf and docs/PATENT-LICENSE.md.
- [ ] Add provenance: docs/PROVENANCE.md + scripts/generate_provenance.sh.
- [ ] Add schemas/ and wire build to emit schemas/provenance.template.json.
- [ ] Tag `v1.0.0-oss`; flip to public; add topics.
