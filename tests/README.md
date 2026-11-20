# Scraper tests

`tests/run.sh` builds `ScraperTests.cpp` natively and runs every site
definition (`src/scraper/*/…Scraper.cpp`) against saved copies of real pages in
`fixtures/`. It needs g++, gcc and the SDL2 + SDL2_ttf headers
(`libsdl2-dev libsdl2-ttf-dev`). CI runs it on every push
(`.github/workflows/tests.yml`).

Each expected value was checked against the saved page itself, not against
what an earlier scraper returned.

## When a site changes its layout

1. Re-save the affected page(s), e.g.

   ```sh
   curl -fsSL -A "Mozilla/5.0" https://hexrom.com/roms/gameboy-advance/ -o tests/fixtures/hexrom/list_gba.html
   ```

2. Run `tests/run.sh`. Failures name the value that's now missing or wrong.
3. Fix the selectors in that site's definition, and update expected values
   that legitimately changed (counts, titles on the page).

| Fixture | Page |
|---|---|
| `hexrom/consoles.html` | https://hexrom.com/rom-category/ |
| `hexrom/list_gba.html` | https://hexrom.com/roms/gameboy-advance/ |
| `hexrom/details_firered.html` | https://hexrom.com/pokemon-fire-red/ |
| `hexrom/download_firered.html` | https://hexrom.com/pokemon-fire-red/download/ |
| `hexrom/search_minish.html` | https://hexrom.com/?s=minish |
| `romspedia/consoles.html` | https://www.romspedia.com/roms |
| `romspedia/list_gba.html` | https://www.romspedia.com/roms/gameboy-advance |
| `romspedia/search_pokemon.html` | https://www.romspedia.com/search?search_term_string=pokemon |
| `romspedia/details_emerald.html` | https://www.romspedia.com/roms/gameboy-advance/pokemon-emerald-usa |
| `romspedia/download_emerald.html` | https://www.romspedia.com/roms/gameboy-advance/pokemon-emerald-usa/download |
| `github/releases.html` | https://github.com/StefanAngelovski/Plunder/releases/ |
