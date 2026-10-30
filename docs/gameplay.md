# Gameplay

All numbers on this page come straight from the source. Times are in **game seconds**, which equal real seconds
unless you build with `DEBUG_FAST` (60x). Most balance constants live at the top of
[`src/pet.cpp`](../src/pet.cpp).

## Life cycle

```
Egg -> Baby -> Child -> Teen -> Adult (22 possible forms)
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

The pet's condition is shown as up to two text labels under the stat bars (on the home screen and in every room),
most important first. They are shown even while it sleeps:

| Label | When |
|-------|------|
| `SLEEPING` | asleep |
| `FEELS ILL` | sick |
| `STARVING!` / `FAMISHED!` / `HUNGRY!` | food at 0 / under 15 / under 30 |
| `VERY SAD!` / `UNHAPPY!` | joy (the heart bar) at 0 / under 30 |
| `EXHAUSTED` / `SLEEPY!` | energy under 10 / under 20, awake |
| `TOO DIRTY!` | 2 or more poop piles |
| `GRUMPY!` | a tantrum |

A repeating beep (every 45 s) and the red LED call you for the most important awake need, in this order:
**sick, hungry** (food < 30), **poop** (2 or more piles), **tantrum**, **bored** (joy < 30), **sleepy**
(energy < 20). Note that "bored" and "unhappy" come from the **joy** (heart) bar, not from energy.

A need that stays unanswered for **15 minutes** is a **care mistake** (once per call; tantrums have their own rule).
Care mistakes push the evolution toward the less rewarding forms.

The **Stats** screen (main menu) shows every number, and a "what to do" list.

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

Decided when the teen stage ends. The first matching row wins. `CM` is care mistakes, `D` discipline, `B` bond,
`J` joy, `E` energy and `F` food at that moment. Higher thresholds are rarer, so the best forms need excellent care.

| # | Condition | Form |
|---|-----------|------|
| 1 | CM = 0, D >= 85, B >= 85, J >= 80 | **Jade Dragon** (the rarest) |
| 2 | CM <= 1, D >= 70, B >= 70 | **Kitsune** (nine-tail fox) |
| 3 | CM <= 1, D >= 70, E >= 70 | **Crane** |
| 4 | CM <= 1, D >= 70 | **Sage Toad** |
| 5 | CM <= 1, D < 70, B >= 60, J >= 70 | **Moon Rabbit** |
| 6 | CM <= 1, D < 70, B >= 60 | **Panda** |
| 7 | CM <= 1, D < 70, J >= 70 | **Koi Spirit** |
| 8 | CM <= 1, D < 70 | **Raiju** (lightning weasel) |
| 9 | CM <= 3, D >= 70, B >= 50 | **Cloud Sheep** |
| 10 | CM <= 3, D >= 70, F >= 70 | **Tortoise** |
| 11 | CM <= 3, D >= 70 | **Sage Toad** |
| 12 | CM <= 3, D < 70, J >= 70, B >= 50 | **Capybara** |
| 13 | CM <= 3, D < 70, J >= 70 | **Axolotl** |
| 14 | CM <= 3, D < 70 | **Tanuki** |
| 15 | CM <= 5, E >= 70 | **Frost Penguin** |
| 16 | CM <= 5, E >= 50 | **Hawk** |
| 17 | CM <= 5, J >= 50 | **Octopus** |
| 18 | CM <= 5, F >= 60 | **Hedgehog** |
| 19 | CM <= 5 | **Moon Slug** |
| 20 | any CM, D >= 50, E >= 60 | **Red Panda** |
| 21 | any CM, D >= 50 | **Shadow Cat** |
| 22 | any CM, D < 50, B >= 40 | **Bat** |
| 23 | any CM, D < 50 | **Serpent** |

(Rows 4 and 11 are the same form, reached with different care, so there are 22 forms in 23 rows.)

Raise **discipline** by scolding tantrums (+12 each), using training scrolls (+10), shuriken training and the
training mission. Raise **bond** by playing, completing missions and tapping the pet. Keep **joy** high (treats and play)
for the rabbit, koi, capybara and axolotl, and keep it **fed** for the tortoise and hedgehog. The DEX shows the
condition for every form once you tap its card.

## Animations

Every stage and form has its own set of 12 frames, chosen from what the pet is doing:

| State | Frames |
|-------|--------|
| Idle | a gentle loop with a blink every few seconds |
| Sitting | now and then on the home screen; whenever it pauses while wandering in a room |
| Walking | two-frame walk cycle while it roams a room (it turns to face the way it walks) |
| Sleeping | curled up, slowly breathing |
| Eating | bite and chew while a food item drops in |
| Being cleaned | lathered in soap foam, then shaking the water off, with soap bubbles floating up |
| Happy | after play, treats, and now and then when all stats are high |
| Sad | when a stat is low, hungry, sick or bored |

The egg wobbles and cracks, and the ghost floats.

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

15 items in two categories, shown four per page with BACK / NEXT. Prices and effects:

**Food** (any food: food is refused above 95; the energy and joy gains still apply to drinks)

| Item | Price | Food | Joy | Energy |
|------|-------|------|-----|--------|
| Berry | 6 | +10 | +6 | |
| Onigiri | 8 | +35 | +3 | |
| Mochi | 10 | +15 | +12 | |
| Dango | 12 | +8 | +20 | |
| Matcha | 12 | +5 | +4 | +20 |
| Taiyaki | 14 | +25 | +12 | |
| Tempura | 16 | +40 | +6 | |
| Udon | 18 | +45 | | +5 |
| Ramen | 20 | +50 | +8 | |
| Sushi | 22 | +45 | +10 | |
| Cake | 26 | +20 | +30 | |
| Bento | 30 | +70 | +8 | +5 |

**Items**

| Item | Price | Effect |
|------|-------|--------|
| Chakra | 15 | Energy +40 (refused above 90) |
| Medicine | 25 | Cures sickness |
| Scroll | 30 | Discipline +10 |

The bag holds up to 9 of each. **Rice** from the Feed room is free: food +20, joy +3. The Feed list shows only the
foods you own, four per page. An item you cannot use right now (for example food while full) is not consumed.

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

A permanent album of the 22 adult forms, six per page (four pages). Unlocked forms show their sprite; locked ones a silhouette. Tapping a card
shows the evolution hint. The DEX also counts how many adults you have raised in total. It survives deaths.
