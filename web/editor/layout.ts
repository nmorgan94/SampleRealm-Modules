export interface Rect {
  left: number;
  top: number;
  width: number;
  height: number;
}

// juce::Rectangle::withSizeKeepingCentre
export const centred = (outer: number, inner: number) => Math.trunc((outer - inner) / 2);

// LabeledSlider::labelHeight
export const LABEL_HEIGHT = 24;

interface LabeledLayout {
  control: Rect;
  label: Rect;
}

// controls/LabeledSlider.h's layOutAboveLabel: the label right under a square control, the pair centred
export function layOutAboveLabel(width: number, height: number, labelHeight = LABEL_HEIGHT): LabeledLayout {
  const size = Math.max(0, Math.min(width, height - labelHeight));
  const top = centred(height, size + labelHeight);
  return {
    control: { left: centred(width, size), top, width: size, height: size },
    label: { left: 0, top: top + size, width, height: labelHeight },
  };
}

// LabeledSlider::NamePlacement::above: the label on top, the control taking the room under it
export function layOutBelowLabel(width: number, height: number, labelHeight = LABEL_HEIGHT): LabeledLayout {
  return {
    control: { left: 0, top: labelHeight, width, height: Math.max(0, height - labelHeight) },
    label: { left: 0, top: 0, width, height: labelHeight },
  };
}
