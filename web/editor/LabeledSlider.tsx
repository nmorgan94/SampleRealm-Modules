// Mirrors editor/LabeledSlider.h and how LookAndFeel_V4 draws it
import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react';
import type { PluginBackend } from '../backend.ts';
import { juceHeight } from './fonts.ts';
import { useParam } from './hooks.ts';
import { layOutAboveLabel, type Rect } from './layout.ts';
import { formatValue, fromProportion, parseValue, toProportion } from './params.ts';

// juce::Slider's defaults
const ROTARY_START = 1.2 * Math.PI;
const ROTARY_END = 2.8 * Math.PI;
const PIXELS_FOR_FULL_DRAG = 250;

// Angles run clockwise from 12 o'clock, as in juce::Path::addCentredArc
const pointAt = (cx: number, cy: number, radius: number, angle: number) =>
  [cx + radius * Math.sin(angle), cy - radius * Math.cos(angle)] as const;

const arcPath = (cx: number, cy: number, radius: number, from: number, to: number) => {
  const [x1, y1] = pointAt(cx, cy, radius, from);
  const [x2, y2] = pointAt(cx, cy, radius, to);
  return `M ${x1} ${y1} A ${radius} ${radius} 0 ${to - from > Math.PI ? 1 : 0} 1 ${x2} ${y2}`;
};

function RotaryGraphic({ width, height, proportion }: { width: number; height: number; proportion: number }) {
  const inner = { width: width - 20, height: height - 20 };
  const cx = width / 2;
  const cy = height / 2;
  const radius = Math.min(inner.width, inner.height) / 2;
  const lineWidth = Math.min(8, radius * 0.5);
  const arcRadius = radius - lineWidth * 0.5;
  const toAngle = ROTARY_START + proportion * (ROTARY_END - ROTARY_START);
  const [dotX, dotY] = pointAt(cx, cy, arcRadius, toAngle);

  return (
    <svg width={width} height={height} aria-hidden>
      <path d={arcPath(cx, cy, arcRadius, ROTARY_START, ROTARY_END)} stroke="var(--color-widget)" strokeWidth={lineWidth} strokeLinecap="round" fill="none" />
      {proportion > 0 && (
        <path d={arcPath(cx, cy, arcRadius, ROTARY_START, toAngle)} stroke="var(--color-thumb)" strokeWidth={lineWidth} strokeLinecap="round" fill="none" />
      )}
      <circle cx={dotX} cy={dotY} r={lineWidth * 0.35} fill="var(--color-fg)" />
    </svg>
  );
}

type DragStart = { x: number; y: number; proportion: number };

// editor/LabeledSlider.h
const NAME_DELAY_MS = 600;

// LabeledSlider::styleLabel, as the props of the label's element
export const styleLabel = (box: Rect) => ({
  className: 'absolute flex items-center justify-center text-center font-bold leading-none',
  style: { ...box, fontSize: juceHeight(16) },
});

interface LabeledSliderProps {
  backend: PluginBackend;
  id: string;
  label: string;
  width: number;
  height: number;
}

// A Slider::RotaryHorizontalVerticalDrag with its name below, which shows the value while it's being moved. Click the name to type a value.
export function LabeledSlider({ backend, id, label, width, height }: LabeledSliderProps) {
  const { value, set, commit, param } = useParam(backend, id);
  const drag = useRef<DragStart | null>(null);
  const nameTimer = useRef<number>();
  const [showValue, setShowValue] = useState(false);
  const [editing, setEditing] = useState(false);
  const proportion = toProportion(param, value);
  const text = param.unit ? `${formatValue(param, value)} ${param.unit}` : formatValue(param, value);

  const layout = layOutAboveLabel(width, height);
  const nameLabel = styleLabel(layout.label);

  useEffect(() => () => clearTimeout(nameTimer.current), []);

  const holdValue = () => {
    clearTimeout(nameTimer.current);
    setShowValue(true);
  };

  const releaseValue = () => {
    clearTimeout(nameTimer.current);
    nameTimer.current = window.setTimeout(() => setShowValue(false), NAME_DELAY_MS);
  };

  const flashValue = () => {
    setShowValue(true);
    releaseValue();
  };

  const end = () => {
    if (!drag.current) return;
    drag.current = null;
    backend.endGesture(id);
    releaseValue();
  };

  const commitText = (typed: string) => {
    setEditing(false);
    const parsed = parseValue(param, typed);
    if (parsed !== null) commit(parsed);
    flashValue();
  };

  return (
    <div className="relative" style={{ width, height }}>
      <div
        className="absolute cursor-pointer touch-none"
        style={layout.control}
        role="slider"
        aria-label={param.name}
        aria-valuemin={param.min}
        aria-valuemax={param.max}
        aria-valuenow={value}
        aria-valuetext={text}
        onPointerDown={(e: ReactPointerEvent) => {
          if (e.button !== 0) return;
          e.currentTarget.setPointerCapture(e.pointerId);
          drag.current = { x: e.clientX, y: e.clientY, proportion };
          backend.beginGesture(id);
          holdValue();
        }}
        onPointerMove={(e: ReactPointerEvent) => {
          const start = drag.current;
          if (!start) return;
          const moved = e.clientX - start.x - (e.clientY - start.y);
          const next = fromProportion(param, start.proportion + moved / PIXELS_FOR_FULL_DRAG);
          if (next !== value) set(next);
        }}
        onPointerUp={end}
        onPointerCancel={end}
        // As SliderAttachment's setDoubleClickReturnValue
        onDoubleClick={() => commit(param.defaultValue)}
      >
        <RotaryGraphic width={layout.control.width} height={layout.control.height} proportion={proportion} />
      </div>
      {editing ? (
        <input
          className={`${nameLabel.className} border border-line bg-widget text-fg outline-none`}
          style={nameLabel.style}
          autoFocus
          defaultValue={text}
          onFocus={(e) => e.currentTarget.select()}
          onBlur={(e) => commitText(e.currentTarget.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') e.currentTarget.blur();
            if (e.key === 'Escape') {
              setEditing(false);
              releaseValue();
            }
          }}
        />
      ) : (
        <div
          className={`${nameLabel.className} cursor-text`}
          style={nameLabel.style}
          onClick={() => {
            clearTimeout(nameTimer.current);
            setEditing(true);
          }}
        >
          {showValue ? text : label}
        </div>
      )}
    </div>
  );
}
