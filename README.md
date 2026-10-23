# Miniature Pip-Boy for M5Stack StickS3

A pocket Fallout-style **Pip-Boy** that runs on an [M5Stack StickS3](https://docs.m5stack.com/en/core/M5StickS3). A green CRT UI with tabs you flip through: a character on the STAT screen whose mood follows the battery, QR codes to your links, and a Wi-Fi "Geiger counter."

Built by **SuperDidZero**. The vault-dweller figure is my own cartoon avatar, left in so you get the same little guy; swap the links and handle for your own (see below).

## What it does

- **STAT** — a walking vault-dweller + your handle, with HP = the real battery level along the bottom. He reacts to the battery:
  - above 50%: walks, winks, throws a thumbs up
  - 20–50%: slumped and panting
  - under 20%: collapsed on the floor
- **COMMS / BASE / RADIO** — three QR codes (LinkedIn, website, Instagram), each playing a short public-domain tune when you open it.
- **RAD** — a Geiger counter driven by the Wi-Fi signal around you (needle + clicks).

## Controls

- **Front button** — next tab
- **Side button** — previous tab
- **Shake** — mute / unmute

## Build it

1. Put `Pip_Boy.ino` and `sprites.h` together in a folder named `Pip_Boy`.
2. Install the **M5Unified** library (Arduino Library Manager). Nothing else is needed — the QR codes are drawn by M5GFX.
3. Board: **M5StickS3**.
4. Upload. If it won't connect, hold the side button until the green light blinks (download mode), pick the port, and upload again.

## Make it yours

Near the top of `Pip_Boy.ino`, change these to your own:

```c
const char* URL_INSTAGRAM = "https://instagram.com/YOUR_HANDLE";
const char* URL_LINKEDIN  = "https://linkedin.com/in/YOUR_LINKEDIN";
const char* URL_WEBSITE   = "https://YOUR-WEBSITE.example";
const char* HANDLE        = "@YOUR_HANDLE";
```

To use your own character, replace the sprites in `sprites.h` (RGB565, byte-swapped for the panel, `0x1FF8` as the transparent colour).

## License

MIT. The figure art is my own; feel free to use the code, and swap in your own character if you'd rather not ship mine.
