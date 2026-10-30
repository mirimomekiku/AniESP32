# Gameplay

All numbers on this page come straight from the source. Times are in **game seconds**, which equal real seconds
unless you build with `DEBUG_FAST` (60x). Most balance constants live at the top of
[`src/pet.cpp`](../src/pet.cpp).

## Life cycle

```
Egg -> Baby -> Child -> Teen -> Adult (8 possible forms)
                 \_______________/
                  neglect -> Ghost -> tap the spirit for a new egg
```

| Stage | Duration | Notes |
|-------|----------|-------|
| Egg | 2 min, or 10 taps | Each tap adds 10 warmth; it hatches at 100. The egg wobbles more as it warms |
| Baby | 30 min | |
| Child | 6 h | Tantrums and missions start here |
| Teen | 24 h | At the end it **evolves**: the adult form is chosen from how you cared for it |
| Adult | no limit | |
| Ghost | until you tap it | A new egg starts; ryo, bag and DEX are kept |

## Stats

| Stat | Range | Meaning |
|------|-------|---------|
| Food, Joy, Energy | 0-100 | The three bars at the top of the home screen (5 segments each, turns red at the last one) |
| Discipline | 0-100 | Hidden. Starts at 40. Decides the evolution |
| Bond | 0-100 | Hidden. Starts at 30. Decides the evolution |
| Care mistakes | counter | Hidden (shown as `CM` on the home screen) |

### Rates while awake / asleep

| | Awake | Asleep |
|--|-------|--------|
| Food | -0.010 per s (full to empty in ~2 h 47 min) | -0.002 per s |
| Joy | -0.007 per s, **-0.020** when food < 25 or sick | unchanged |
| Energy | -0.004 per s | **+0.03 per s** |

## Needs and care mistakes

The pet calls for attention with a blinking bubble above it, a repeating beep (every 45 s) and the red LED. The need
shown is the highest priority one:

1. **Sick** 2. **Hungry** (food < 30) 3. **Poop** (2 or more piles) 4. **Tantrum** 5. **Bored** (joy < 30)
6. **Sleepy** (energy < 20 while awake)

A need that stays unanswered for **15 minutes** is a **care mistake** (once per call; tantrums have their own rule).
Care mistakes push the evolution toward the less rewarding forms.

### Poop

Appears every 40-90 minutes while awake (random), up to 4 piles. Clean them in the **Clean** room. With **3 or more**
piles for 15 minutes the pet gets sick.

### Sickness

Caused by 3+ poop piles for 15 minutes, or food at 0 for 15 minutes. Cure it with **medicine** (Clean room, or the
Bag). Untreated for **90 minutes** it turns into a ghost. Medicine is bitter: joy -5. While sick, joy drains about
three times faster.

### Tantrums

From the child stage, awake, every 2-4 hours. **Tap the pet** within 5 minutes to scold it (+12 discipline, +5 ryo).
Ignoring it costs a care mistake and 5 discipline.

## Sleeping

- You can put the pet to bed or wake it in the **Sleep** room. Waking a sleeping pet with a tap costs 5 joy.
- **At night (21:00-06:00 by the clock)** it falls asleep if energy is under 70, and sleeps until morning. If you wake
  it on purpose it stays up until the night ends.
- **Lights out:** if the light sensor reads dark for ~20 s it goes to sleep, and wakes ~5 s after the light returns
  (only if it was put to bed by the dark). If you wake it in the dark it stays awake.
- By day it wakes by itself when energy reaches 100.
- When energy reaches 0 while awake it falls asleep from exhaustion.
- The **alarm** wakes a sleeping pet (joy -2).

## Evolution

Decided when the teen stage ends. The first matching row wins. `CM` is care mistakes, `D` discipline, `B` bond.

| # | Condition | Form |
|---|-----------|------|
| 1 | CM <= 1, D >= 70, B >= 70 | **Kitsune** (nine-tail fox) |
| 2 | CM <= 1, D >= 70 | **Sage Toad** |
| 3 | CM <= 1, D < 70 | **Raiju** (lightning weasel) |
| 4 | CM <= 3, D >= 70 | **Sage Toad** |
| 5 | CM <= 3, D < 70 | **Tanuki** |
| 6 | CM <= 5, Energy >= 50 | **Hawk** |
| 7 | CM <= 5, Energy < 50 | **Moon Slug** |
| 8 | any CM, D >= 50 | **Shadow Cat** |
| 9 | any CM, D < 50 | **Serpent** |

Raise **discipline** by scolding tantrums (+12 each), using training scrolls (+10), shuriken training and the
training mission. Raise **bond** by playing, completing missions and tapping the pet.

## Economy

Currency is **ryo**. You start with 30 ryo, 1 medicine and 1 onigiri.

| Source | Ryo |
|--------|-----|
| Hatching | +10 |
| Each evolution | +30 |
| Quick play | +3 |
| Cleaning | +2 per pile |
| Scolding a tantrum | +5 |
| Errands | +1 per 30 min awake (child and up) |
| Missions and mini-games | see below |

### Shop

| Item | Price | Effect |
|------|-------|--------|
| Onigiri | 8 | Food +35, joy +3 |
| Ramen | 20 | Food +50, joy +8 |
| Dango | 12 | Food +8, joy +20 |
| Chakra | 15 | Energy +40 (refused above 90) |
| Medicine | 25 | Cures sickness |
| Scroll | 30 | Discipline +10 |

The bag holds up to 9 of each. **Rice** from the Feed room is free: food +20, joy +3. Food is refused above 95.

## Actions in detail

| Action | Effect |
|--------|--------|
| Quick play | Joy +18, energy -10, food -4, bond +1, ryo +3. Needs energy >= 10 and food > 0 |
| Clean | Removes all poop, joy +2 |
| Tap the pet | Bond +0.5 and joy +1 (or scolds a tantrum, wakes a sleeper, warms an egg) |

## Mini-games

Both need an awake, healthy pet (not sick, food >= 10, energy >= 15). Open them from **Play**.

### Shuriken

5 throws. The target slides left and right, getting 28 px/s faster after every throw (starts at 70 px/s). Tap to throw;
the shuriken takes 280 ms to arrive, so aim where the target **will** be in front of you.

| Distance from centre on arrival | Points |
|--------------------------------|--------|
| < 8 px | 3 (bullseye) |
| < 18 px | 2 |
| < 30 px | 1 |
| otherwise | 0 |

Reward: **ryo = 2 x score**, joy +score, discipline +score/3, energy -15, food -4. A score of 12 or more also gives
+1 bond.

### Shell game

The pet hides under one of three smoke clouds, the clouds shuffle, and you tap the one hiding it. 3 rounds with
5, 7 and 9 swaps, getting faster each round.

Reward per win: **6 ryo** and 5 joy. Energy -8, food -4 for the session. A perfect game gives +1 bond.

## Missions

The pet leaves home and comes back with rewards. Needs a child-or-older pet that is awake, **not sick, food >= 30,
energy >= 30**. While away the village shows a lantern and a countdown, and the pet has no poop, needs or tantrums.
Each mission also gives +1 bond.

| Mission | Time | Ryo | Joy | Discipline | Energy | Food |
|---------|------|-----|-----|------------|--------|------|
| Errand | 5 min | +8 | | | -5 | -3 |
| Scout | 15 min | +20 | +10 | | -15 | -8 |
| Training | 30 min | +15 | | +10 | -25 | -12 |

Missions are saved: a reboot does not cancel them.

## Offline catch-up

If the device was switched off, it simulates the missed time **once**, but only when an NTP sync succeeds (otherwise
there is no trustworthy clock): the gap since the last save, **capped at 8 hours**. No sounds or pop-ups play for the
simulated time.

## DEX

A permanent album of the 8 adult forms. Unlocked forms show their sprite; locked ones a silhouette. Tapping a card
shows the evolution hint. The DEX also counts how many adults you have raised in total. It survives deaths.
