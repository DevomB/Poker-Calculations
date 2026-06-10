import { readFileSync, writeFileSync, unlinkSync } from 'node:fs';

const helpers = `
[[nodiscard]] int hand_rank_order(HandRank r) {
    return static_cast<int>(r);
}

[[nodiscard]] bool rank_at_least(HandRank r, HandRank min_r) {
    return hand_rank_order(r) >= hand_rank_order(min_r);
}

[[nodiscard]] double union_outs_two(double oa, double ob, double shared) {
    return oa + ob - shared;
}

[[nodiscard]] double union_outs_three(double oa, double ob, double oc, double sab, double sac,
                                     double sbc, double sabc) {
    return oa + ob + oc - sab - sac - sbc + sabc;
}

[[nodiscard]] double union_outs_four(double oa, double ob, double oc, double od, double s01,
                                   double s02, double s03, double s12, double s13, double s23,
                                   double s012, double s013, double s023, double s123,
                                   double four_way) {
    return oa + ob + oc + od - s01 - s02 - s03 - s12 - s13 - s23 + s012 + s013 + s023 + s123 -
           four_way;
}

[[nodiscard]] double final_pot_after_hu_call(double pot_before_call, double to_call) {
    return pot_before_call + 2.0 * to_call;
}

[[nodiscard]] double final_pot_after_hu_bet(double pot_before_bet, double bet_size) {
    return pot_before_bet + 2.0 * bet_size;
}
`;

const rec = readFileSync('src/_recovered_math.cpp', 'utf8');
let main = readFileSync('src/poker_math.cpp', 'utf8');
if (!main.includes('union_outs_two')) {
  main = main.replace('}  // namespace\n\ndouble pot_odds_ratio', `${helpers}\n}  // namespace\n\ndouble pot_odds_ratio`);
}
const extFuncs = rec.replace(/^[\s\S]*?^}  \/\/ namespace\n\n/m, '').replace(/\n\}  \/\/ namespace poker\n$/, '');
main = main.replace(/\n\}  \/\/ namespace poker\n$/, `\n${extFuncs}\n}  // namespace poker\n`);
writeFileSync('src/poker_math.cpp', main);
unlinkSync('src/_recovered_math.cpp');
unlinkSync('scripts/_merge-recovered.mjs');
console.log('merged implementations into poker_math.cpp');
