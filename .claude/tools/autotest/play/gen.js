// Writes the Fallout 2 "play it while he is away" scenarios (2026-10-23, his "je možné že bys odehrál
// Fallout 2 na pozadí, zatímco budu pryč, abys zjistil, co je špatně a co je dobře?") into this folder.
// node gen.js -- then run them with playall.sh. F2 pids from pro_crit.msg / pro_item.msg.
// ⚠️ `item <pid> <qty>` gives qty ITEMS -- for ammo, qty full BOXES. Dozens of boxes overload a critter,
// and Fallout 2 takes a max AP per 40 lb over (stat.cc): the first batch fired a third as often.
const fs = require('fs');
const path = require('path');
const D = __dirname;

const RAIDER = 16777454; // art cannot hold a knife
const RAIDER2 = 16777455; // holds a knife
const DUDE = 'dude tile 20100 rot 2';

function write(name, lines) {
    fs.writeFileSync(path.join(D, name + '.txt'), lines.join('\n') + '\n');
}

// --- the dude's weapons: one scenario each, three raiders coming -------------------------------------
const weapons = [
    // name, wield, ammo [pid qty], skill, extra dude lines, enemies, policy
    ['w-smg', 9, [29, 4], 'small_guns', [], 'pistol', 'fight'],
    ['w-shotgun', 94, [95, 4], 'small_guns', [], 'knife', 'fight'],
    ['w-huntrifle', 10, [34, 4], 'small_guns', [], 'pistolfar', 'fight'],
    ['w-assault', 23, [35, 4], 'small_guns', [], 'pistol', 'fight'],
    ['w-minigun', 12, [35, 6], 'big_guns', ['dude stat ST 10'], 'pistol', 'fight'],
    ['w-flamer', 11, [32, 3], 'big_guns', ['dude stat ST 8'], 'knife', 'fight'],
    ['w-rocket', 13, [14, 8], 'big_guns', ['dude stat ST 8'], 'pistolfar', 'fight'],
    ['w-laserpistol', 16, [38, 3], 'energy', [], 'pistol', 'fight'],
    ['w-plasmarifle', 15, [39, 3], 'energy', [], 'pistol', 'fight'],
    ['w-eagle', 18, [31, 4], 'small_guns', [], 'pistol', 'fight'],
    ['w-sledge', 6, null, 'melee', ['dude stat ST 10'], 'pistol', 'advance'],
    ['w-spear', 7, null, 'melee', [], 'pistol', 'advance'],
    ['w-powerfist', 235, [38, 3], 'unarmed', [], 'pistol', 'advance'],
    ['w-fists', -1, null, 'unarmed', [], 'pistol', 'advance'],
    ['w-throwknife', 45, null, 'throwing', ['dude item 45 12'], 'pistol', 'fight'],
];
const enemySets = {
    pistol: [`spawn ${RAIDER} 22500 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 22496 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 22504 rot 5 wield 8 item 29 2 hostile`],
    pistolfar: [`spawn ${RAIDER} 23300 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 23296 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 23304 rot 5 wield 8 item 29 2 hostile`],
    knife: [`spawn ${RAIDER2} 23100 rot 5 wield 4 hostile`, `spawn ${RAIDER2} 23096 rot 5 wield 4 hostile`, `spawn ${RAIDER2} 23104 rot 5 wield 4 hostile`],
};
for (const [name, wield, ammo, skill, extra, enemies, policy] of weapons) {
    const l = [`# F2 play-through: the dude's ${name.slice(2)} against three raiders.`, `name ${name}`, 'map DESERT1.MAP', 'seed 1', 'duration 40', DUDE,
        'dude stat PE 8', 'dude stat AG 8', 'dude stat EN 8', 'dude hp 250', `dude skill ${skill} 130`, 'dude armor 2', ...extra];
    if (wield > 0) l.push(`dude wield ${wield}`);
    if (ammo) l.push(`dude item ${ammo[0]} ${ammo[1]}`);
    l.push('dude item 40 4', '', ...enemySets[enemies], '', `at 0 ${policy}`);
    write(name, l);
}

// the plasma grenade from the RB slot, thrown three times
write('w-plasmagrenade', ['# F2 play-through: plasma grenades from the RB quick slot.', 'name w-plasmagrenade', 'map DESERT1.MAP', 'seed 1', 'duration 30', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude hp 250', 'dude skill throwing 130', 'dude armor 2', 'dude wield 8', 'dude item 29 2', 'dude item 26 4', 'dude quickslot rb 26', '',
    `spawn ${RAIDER} 21700 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 21702 rot 5 wield 8 item 29 2 hostile`, '',
    'at 0 throw', 'at 6 throw', 'at 12 throw', 'at 18 fight']);

// --- Fallout 2's creatures against a well armed dude -------------------------------------------------
const creatures = [
    ['c-geckos', [[16777296, 22500], [16777296, 22504], [16777299, 22900]]],
    ['c-firegeckos', [[16777456, 22500], [16777456, 22504]]],
    ['c-deathclaw', [[16777223, 23100]]],
    ['c-floaters', [[16777462, 22500], [16777462, 22504]]],
    ['c-centaurs', [[16777464, 22500], [16777464, 22504]]],
    ['c-aliens', [[16777458, 22500], [16777458, 22504]]],
    ['c-scorpions', [[16777221, 22500], [16777221, 22504]]],
    ['c-ants', [[16777598, 22100], [16777598, 22104], [16777598, 22300], [16777599, 22500]]],
    ['c-rats', [[16777328, 22100], [16777328, 22104], [16777326, 22300]]],
    ['c-robots', [[16777291, 22500], [16777388, 22504]]],
    ['c-cyberdogs', [[16777352, 22500], [16777352, 22504]]],
    ['c-dogs', [[16777298, 22100], [16777298, 22104], [16777298, 22300]]],
    ['c-spores', [[16777232, 21300], [16777232, 21304], [16777232, 21500]]],
    ['c-turret', [[16777686, 21700]]],
    ['c-brahmin', [[16777435, 22100]]],
];
for (const [name, list] of creatures) {
    const l = [`# F2 play-through: ${name.slice(2)} (their own attacks and animations) against an armed dude.`, `name ${name}`, 'map DESERT1.MAP', 'seed 1', 'duration 45', DUDE,
        'dude stat PE 8', 'dude stat AG 8', 'dude stat EN 8', 'dude hp 300', 'dude skill small_guns 150', 'dude armor 17', 'dude wield 23', 'dude item 35 4', 'dude item 40 4', ''];
    for (const [pid, tile] of list) l.push(`spawn ${pid} ${tile} rot 5 hostile`);
    l.push('', 'at 0 fight');
    write(name, l);
}
// armed creatures: super mutants with a minigun and a rocket launcher, Enclave with plasma
write('c-mutants', ['# F2 play-through: super mutants -- a minigun and a rocket launcher.', 'name c-mutants', 'map DESERT1.MAP', 'seed 1', 'duration 45', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude stat EN 8', 'dude hp 400', 'dude skill small_guns 150', 'dude armor 17', 'dude wield 23', 'dude item 35 4', 'dude item 40 4', '',
    'spawn 16777335 23100 rot 5 wield 12 item 35 4 hostile', 'spawn 16777336 23500 rot 5 wield 13 item 14 4 hostile', '', 'at 0 fight']);
write('c-enclave', ['# F2 play-through: an Enclave patrol with plasma rifles.', 'name c-enclave', 'map DESERT1.MAP', 'seed 1', 'duration 45', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude stat EN 8', 'dude hp 400', 'dude skill small_guns 150', 'dude armor 17', 'dude wield 23', 'dude item 35 4', 'dude item 40 4', '',
    'spawn 16777466 23100 rot 5 wield 15 item 39 2 hostile', 'spawn 16777466 23104 rot 5 wield 15 item 39 2 hostile', '', 'at 0 fight']);
write('c-slavers-burst', ['# F2 play-through: slavers with SMGs (bursts) and a shotgun.', 'name c-slavers-burst', 'map DESERT1.MAP', 'seed 1', 'duration 45', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude stat EN 8', 'dude hp 300', 'dude skill small_guns 150', 'dude armor 17', 'dude wield 23', 'dude item 35 4', 'dude item 40 4', '',
    'spawn 16777433 22500 rot 5 wield 9 item 29 3 hostile', 'spawn 16777433 22504 rot 5 wield 9 item 29 3 hostile', 'spawn 16777440 22900 rot 5 wield 94 item 95 3 hostile', '', 'at 0 fight']);

// --- allies on the dude's side (team 0): do they fight in realtime? -----------------------------------
write('p-allies', ['# F2 play-through: Sulik (club) and Vic (pistol) on the dude\'s team against four raiders.', 'name p-allies', 'map DESERT1.MAP', 'seed 1', 'duration 50', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude hp 250', 'dude skill small_guns 120', 'dude armor 2', 'dude wield 8', 'dude item 29 3', 'dude item 40 4', '',
    'spawn 16777313 20302 rot 2 wield 5 team 0', 'spawn 16777278 19902 rot 2 wield 8 item 29 3 team 0', '',
    `spawn ${RAIDER} 22500 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 22496 rot 5 wield 8 item 29 2 hostile`,
    `spawn ${RAIDER2} 22900 rot 5 wield 4 hostile`, `spawn ${RAIDER2} 22904 rot 5 wield 4 hostile`, '', 'at 0 fight']);

// --- a big brawl: many fighters on both sides, a minute long -----------------------------------------
const brawl = ['# F2 play-through: a big brawl -- ten raiders against the dude and four allies, a minute long.', 'name p-brawl', 'map DESERT1.MAP', 'seed 1', 'duration 70', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude hp 400', 'dude skill small_guns 150', 'dude armor 17', 'dude wield 23', 'dude item 35 6', 'dude item 40 6', '',
    'spawn 16777313 20302 rot 2 wield 5 team 0', 'spawn 16777278 19902 rot 2 wield 23 item 35 3 team 0',
    'spawn 16777262 20304 rot 2 wield 9 item 29 3 team 0', 'spawn 16777262 19904 rot 2 wield 94 item 95 3 team 0', ''];
const tiles = [22500, 22496, 22504, 22900, 22894, 22906, 23300, 23296, 23304, 23700];
tiles.forEach((t, i) => brawl.push(i % 3 === 2 ? `spawn ${RAIDER2} ${t} rot 5 wield 4 hostile` : `spawn ${RAIDER} ${t} rot 5 wield ${i % 2 ? 9 : 8} item 29 3 hostile`));
brawl.push('', 'at 0 fight');
write('p-brawl', brawl);

console.log('written', fs.readdirSync(D).filter((f) => f.endsWith('.txt')).length, 'scenarios');

// --- real party members (partyMemberAdd): followers fight on realtime's friendly side ---------------
write('p-party', ['# F2 play-through: Sulik and Vic RECRUITED (party), against four raiders -- do followers fight?', 'name p-party', 'map DESERT1.MAP', 'seed 1', 'duration 50', DUDE,
    'dude stat PE 8', 'dude stat AG 8', 'dude hp 250', 'dude skill small_guns 120', 'dude armor 2', 'dude wield 8', 'dude item 29 4', 'dude item 40 4', '',
    'spawn 16777313 20302 rot 2 wield 5 party', 'spawn 16777278 19902 rot 2 wield 8 item 29 3 party', '',
    `spawn ${RAIDER} 22500 rot 5 wield 8 item 29 2 hostile`, `spawn ${RAIDER} 22496 rot 5 wield 8 item 29 2 hostile`,
    `spawn ${RAIDER2} 22900 rot 5 wield 4 hostile`, `spawn ${RAIDER2} 22904 rot 5 wield 4 hostile`, '', 'at 0 fight']);
console.log('+ p-party');
