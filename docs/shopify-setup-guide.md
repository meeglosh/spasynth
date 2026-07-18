# SPASynth Shopify setup guide

Plain-English, non-technical walkthrough for setting up the SPASynth products
on Shopify. Files to upload live in `dist/shopify/` (gitignored, local only);
the copy to paste lives in `docs/shopify-listings.md`. House style: no em dashes.

## Before you start

1. Shopify admin login (`yourstore.myshopify.com/admin`).
2. The files: `dist/shopify/SPASynth-Standard-1.0.0/` and
   `dist/shopify/SPASynth-Pro-1.0.0/` (open in Finder).
3. The copy: `docs/shopify-listings.md` (titles, descriptions, SEO, blurbs).
4. Prices:

| Product | Price | Compare-at price |
| --- | --- | --- |
| Standard | 99 | 149 |
| Pro | 499 | 899 |
| Upgrade | 400 | 750 |

Price = paid today; Compare-at = the higher struck-through number.

## Part 1: Turn on digital downloads (one time)

1. Settings (bottom-left) -> Apps and sales channels -> Shopify App Store.
2. Search "Digital Downloads", pick the free one by Shopify, Install.
3. Confirm a payment provider exists: Settings -> Payments (Shopify Payments
   or PayPal). Set one up before going live if none.

## Part 2: Create SPASynth Standard

1. Products -> Add product.
2. Title: `SPASynth Standard`.
3. Description: paste the "1 · SPASynth Standard" section from
   `docs/shopify-listings.md`.
4. Media: upload `docs/spasynth-marketing.png` (+ logo optionally).
5. Pricing: Price `99`, Compare-at price `149`.
6. Inventory: UNCHECK "Track quantity".
7. Shipping: UNCHECK "This is a physical product" (critical; removes shipping).
8. Search engine listing -> Edit: paste Standard's Page title + Meta
   description; URL handle `spasynth-standard`.
9. Status: leave on Draft for now.
10. Save.

## Part 3: Attach the files (Standard)

1. Apps -> Digital Downloads -> select SPASynth Standard.
2. Upload these six files from `SPASynth-Standard-1.0.0/`, one at a time:
   - `SPASynth-1.0.0-macOS.pkg`
   - `SPASynth-1.0.0-Windows.exe`
   - `SPASynth Starter Library.zip` (~3 GB, in the `Library` subfolder)
   - `README.txt`, `QUICKSTART.txt`, `EULA.txt`
   Never wrap them in one giant zip; attach individually.

## Part 4: Create SPASynth Pro

The 37 GB Pro library is too big for Shopify, so it lives on Cloudflare R2 and
is delivered as a small links file. You do NOT upload the library to Shopify.

Repeat Parts 2 and 3 with:
- Title `SPASynth Pro`; description = "2 · SPASynth Pro" section.
- Price `499`, Compare-at `899`; URL handle `spasynth-pro`.
- Files from `SPASynth-Pro-1.0.0/` (all small, upload directly): pkg, exe, the 3
  docs, and `SPASynth Pro Library - Download Links.txt`.
- Do NOT upload the 11 zips in the `Library/` subfolder. The library is already
  on R2 at downloads.spasynth.com; the links file gives buyers the download URLs.

## Part 5: Create the Standard to Pro Upgrade

Repeat Parts 2 and 3 with:
- Title `SPASynth Standard to Pro Upgrade`; description = "3 · ... Upgrade".
- Price `400`, Compare-at `750`; URL handle `spasynth-standard-to-pro-upgrade`.
- Files from `SPASynth-Upgrade-1.0.0/`: `SPASynth Pro Library - Download Links.txt`
  + `QUICKSTART.txt` + `EULA.txt`. No pkg/exe (upgraders already own the synth);
  no library upload (it is on R2).
- Honor system: Shopify cannot verify prior Standard ownership; fits the
  no-DRM stance.

## Part 6 (optional): free version for Everything Bundle owners

Business decision of record: Everything Bundle owners get SPASynth free.
- Title `SPASynth for Everything Bundle Owners`; Price `0`.
- Files: pkg + exe + 3 docs only (no library; they own all the sounds).
- Keep it off the public storefront (Draft, or remove the Online Store
  channel) and share via a direct link or a 100%-off code.
- Buyers install it and point it at their existing bundle folder with
  "SET LIBRARY" (no re-download, no reorganizing).

## Part 7: Tax and checkout

- Settings -> Taxes and duties: enable Shopify's handling for EU/UK VAT on
  digital goods if selling there. US is usually untaxed for digital, varies
  by state.
- No shipping settings needed (physical product is unchecked everywhere).

## Part 8: Test before launch (do not skip)

1. Temporarily set one product Active (or use test mode).
2. Buy it yourself; refund afterward.
3. Confirm: download email + links arrive; files download; the macOS
   installer opens with no Gatekeeper warning (signed + notarized); after
   install, SPASynth finds the Starter library and a preset plays.

## Part 9: Go live

1. Each product Status -> Active.
2. Ensure each is on the Online Store sales channel.
3. Add to a Collection if used.
