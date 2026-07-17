import { BattleStreams, RandomPlayerAI, Teams } from '@pkmn/sim';
import { TeamGenerators } from '@pkmn/randoms';

Teams.setGeneratorFactory(TeamGenerators);

function joinBattle(p1Name, p1PackedTeam) {
  const streams = BattleStreams.getPlayerStreams(new BattleStreams.BattleStream());
  const spec = { formatid: 'gen9customgame' };
  const p1spec = { name: p1Name, team: p1PackedTeam };
  const p2spec = { name: 'Bot 2', team: Teams.pack([Teams.generate('gen9randombattle')[0]]) };

  // const p1 = new RandomPlayerAI(streams.p1);
  const p2 = new RandomPlayerAI(streams.p2);

  // void p1.start();
  void p2.start();

  function choose(choice) {
    streams.p1.write(choice);
  }
  globalThis.choose = choose;

  void (async () => {
    for await (const chunk of streams.p1) {
      console.log(chunk);
    }
  })();

  void streams.omniscient.write(`>start ${JSON.stringify(spec)}
>player p1 ${JSON.stringify(p1spec)}
>player p2 ${JSON.stringify(p2spec)}`);

  streams.omniscient.write('>p1 team 1');
}

globalThis.joinBattle = joinBattle;

joinBattle('nerfis', 'Arcanine||Leftovers|Intimidate|Flareblitz,Extremespeed,Wildcharge,Morningsun|Impish|252,0,252,0,4,0||||||||')
