import { Dex } from "@pkmn/dex";

function getSpecies(species) {
  return JSON.stringify(Dex.species.get(species));
}
globalThis.getSpecies = getSpecies;