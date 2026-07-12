import {Dex, BattleStreams, RandomPlayerAI, Teams} from '@pkmn/sim';
import {TeamGenerators} from '@pkmn/randoms';
Teams.setGeneratorFactory(TeamGenerators);

const streams = BattleStreams.getPlayerStreams(new BattleStreams.BattleStream());
const spec = {formatid: 'gen9customgame'};

const p1spec = {name: 'Bot 1', team: Teams.pack([Teams.generate('gen3randombattle')[0]])};
const p2spec = {name: 'Bot 2', team: 'Arcanine||Leftovers|Intimidate|Flareblitz,Extremespeed,Wildcharge,Morningsun|Impish|252,0,252,0,4,0||||||||'};

// const p1 = new RandomPlayerAI(streams.p1);
const p2 = new RandomPlayerAI(streams.p2);

// void p1.start();
void p2.start();

function write(choice) {
  console.log("> " + choice);
  streams.p1.write(choice);
}
globalThis.write = write;

void (async () => {
  for await (const chunk of streams.p1) {
    console.log(chunk);
  }
})();

void streams.omniscient.write(`>start ${JSON.stringify(spec)}
>player p1 ${JSON.stringify(p1spec)}
>player p2 ${JSON.stringify(p2spec)}`);

streams.p1.write("team 1");