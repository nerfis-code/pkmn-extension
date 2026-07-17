import { Dex } from "@pkmn/dex";

function getSpecies(species) {
  const species_data = Dex.species.get(species);

  // incluir formeOrder incluso en una variante, para saber su orden
  if (!("formeOrder" in species_data)) {
    species_data.formeOrder = Dex.species.get(
      species_data.baseSpecies
    ).formeOrder;
  }

  return JSON.stringify(species_data);
}
globalThis.getSpecies = getSpecies;