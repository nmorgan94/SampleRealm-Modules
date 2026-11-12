import type { ParamInfo } from '../backend.ts';

const clamp01 = (value: number) => Math.min(1, Math.max(0, value));

export const fromProportion = (param: ParamInfo, proportion: number) => {
  const p = clamp01(proportion);
  const value = param.min + (param.max - param.min) * (param.skew === 1 ? p : Math.pow(p, 1 / param.skew));
  return snap(param, value);
};

export const toProportion = (param: ParamInfo, value: number) => {
  const p = clamp01((value - param.min) / (param.max - param.min));
  return param.skew === 1 ? p : Math.pow(p, param.skew);
};

// As juce::roundToInt, which integer and choice parameters snap with: halves go to the even number
const roundHalfEven = (value: number) => {
  const rounded = Math.round(value);
  return Math.abs(value % 1) === 0.5 && rounded % 2 !== 0 ? rounded - 1 : rounded;
};

export const snap = (param: ParamInfo, value: number) => {
  const clamped = Math.min(param.max, Math.max(param.min, value));
  if (param.kind === 'integer' || param.kind === 'choice') return roundHalfEven(clamped);
  if (param.step <= 0) return clamped;
  const snapped = param.min + Math.round((clamped - param.min) / param.step) * param.step;
  // Avoids 0.7000000000000001
  return Math.min(param.max, Number(snapped.toFixed(decimalPlaces(param.step))));
};

const decimalPlaces = (step: number) => {
  if (step <= 0) return 7;
  if (Number.isInteger(step)) return 0;

  let places = 7;
  let scaled = Math.abs(Math.round(step * 10 ** places));
  while (scaled % 10 === 0 && places > 0) {
    places--;
    scaled /= 10;
  }
  return places;
};

export const formatValue = (param: ParamInfo, value: number) => {
  if (param.kind === 'choice') return param.choices[Math.round(value)] ?? '';
  if (param.kind === 'integer') return `${Math.round(value)}`;
  // juce::AudioParameterBool's default text
  if (param.kind === 'toggle') return value >= 0.5 ? 'On' : 'Off';

  return value.toFixed(decimalPlaces(param.step));
};

// A choice's name, in any case, before a number: '1/4' is a name, not 1
export const parseValue = (param: ParamInfo, text: string): number | null => {
  const typed = text.trim().toLowerCase();
  const choiceIndex = param.choices.findIndex((choice) => choice.toLowerCase() === typed);
  if (choiceIndex >= 0) return choiceIndex;

  const number = parseFloat(text);
  if (Number.isNaN(number)) return null;
  return snap(param, number);
};
