// @ts-check
import {Dex} from '@pkmn/dex';
import {Generations} from '@pkmn/data';
import { Battle } from '@pkmn/client';
import {Protocol} from '@pkmn/protocol';
import { LogFormatter, ChoiceBuilder } from '@pkmn/view';

const gens = new Generations(Dex);
const battle = new Battle(gens);
const formatter = new LogFormatter(0 /* perspective */, battle);

function handler(scene, chunk) {
  for (const { args, kwArgs } of Protocol.parse(chunk)) {
    // NOTE: must come *before* handler
    scene.anim("fuck_you");
    const formatted = formatter.formatText(args, kwArgs);
    if (formatted) console.log(formatted);
    battle.add(args, kwArgs);
  }
}

globalThis.handler = handler;

// /**
//  * @implements {Protocol.Handler}
//  */
// class BoostHandler {
  // /**
  //  * Maneja el mensaje de protocolo `|-boost|`.
  //  *
  //  * @param {import("@pkmn/protocol").Args['|-boost|']} args
  //  * @param {import("@pkmn/protocol").KWArgs['|-boost|']} kwArgs
  //  */
//   '|-boost|'(args, kwArgs) {
//     const [, p, stat, n] = args;
//     const pokemon = Protocol.parsePokemonIdent(p);
//     const num = Number(n);

//     let message = `${pokemon.player}'s ${pokemon.name}'s ${stat} stat was boosted by ${num}`;
//     if (kwArgs.from) message += ` from ${Protocol.parseEffect(from).name}`;
//     console.log(`${message}!`);
//   }

//   ''() {

//   }
// }