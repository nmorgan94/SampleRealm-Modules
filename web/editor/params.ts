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

export const snap = (param: ParamInfo, value: number) => {
  const clamped = Math.min(param.max, Math.max(param.min, value));
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

  return value.toFixed(decimalPlaces(param.step));
};

export const parseValue = (param: ParamInfo, text: string): number | null => {
  const number = parseFloat(text);
  if (Number.isNaN(number)) return null;
  return snap(param, number);
};
