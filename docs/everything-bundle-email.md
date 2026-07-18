# SPASynth free for Everything Bundle owners

Decision of record: Everything Bundle owners get SPASynth free (part of lifetime
updates). They already own every sound SPASynth plays, so they get the synth
installer only and point it at their existing library with SET LIBRARY. This doc
holds the Shopify discount-code setup and the ready-to-send email. House style:
no em dashes.

## Part 1: Set up the 100%-off discount code

First make sure the **SPASynth for Everything Bundle Owners** product exists
(Part 6 of `docs/shopify-setup-guide.md`): installer-only (pkg + exe + README +
QUICKSTART + EULA, NO library). Give it a real price you would notionally charge
(e.g. `99`) so a random person who stumbles on the link does not get it free, and
publish it to the Online Store but do NOT add it to any collection or menu (so it
is reachable by direct link but not browsable).

Then create the code:

1. Shopify admin, left sidebar -> **Discounts** -> **Create discount** ->
   **Amount off products**.
2. **Method:** select **Discount code** (not Automatic). Type a memorable code
   like `BUNDLEGIFT` (or click Generate).
3. **Value:** **Percentage**, enter **100**.
4. **Applies to:** **Specific products** -> Browse -> select
   **SPASynth for Everything Bundle Owners**. (So the code only zeroes out that
   product, nothing else in the store.)
5. **Minimum purchase requirements:** **No minimum**.
6. **Maximum discount uses:** check "Limit number of times this discount can be
   used in total" and set it a little above your number of bundle owners; also
   check "Limit to one use per customer."
7. **Active dates:** set a start date; leave the end empty or set one to expire.
8. **Save.**

Bundle owners visit the product link, add to cart, enter `BUNDLEGIFT`, and their
total becomes $0.

Simpler alternative: set that product's price to `0` and just email the link. Any
link-holder gets it free, but since the synth is useless without a library that
leakage is basically harmless. The code version just gives tidy control + a cap.

## Part 2: The email

**Subject:** Your SPASynth is ready (free, because you own the Everything Bundle)

**Preheader:** The synth that plays your Silverplatter library. It is yours, no charge.

```
Hi there,

You own the Silverplatter Audio Everything Bundle, which means you already
own every sound our new synth plays. So SPASynth is yours, free, as part of
your lifetime updates.

What it is. SPASynth is a hybrid synthesizer built around your library. Load
any sound from your packs and play it on the keyboard, granulate it into an
evolving texture, or let its own loudness and pitch drive the filter. Your
sounds are not sitting in a slot. They become the instrument.

How to get it.
  1. Go here: [PRODUCT LINK]
  2. Add it to your cart and enter this code at checkout: BUNDLEGIFT
  3. Your total is $0. Complete checkout and download the installer
     (macOS and Windows are both included).

How to connect it to the library you already own. You do not need to
re-download or reorganize anything.
  1. Install and open SPASynth.
  2. In the preset browser, click SET LIBRARY and point it at the folder
     that holds your Silverplatter packs.
  3. That is it. SPASynth reads your existing sounds and builds presets from
     them automatically the next time it opens. The included QUICKSTART has
     the full walkthrough.

No copy protection, no activation, nothing to register. It is the same way we
make everything: we would rather build for people who pay for the tools they
love, and you already have.

Thank you for being a Silverplatter Audio customer from the start. Go make
something that has never existed before.

The Silverplatter Audio team
spasynth.com
```

Notes:
- `[PRODUCT LINK]` = the hidden product's URL; `BUNDLEGIFT` = the code from Part 1.
  Change the code in both places if you pick a different one.
- The "point it at the folder that holds your packs" line assumes the bundle sits
  under one parent folder. Customers with scattered packs: the QUICKSTART and the
  existing-library FAQ cover consolidating them.
