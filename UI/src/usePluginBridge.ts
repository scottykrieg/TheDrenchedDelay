import { useState, useEffect, useCallback, useRef } from 'react';
import { PluginState, defaultState } from '../types/plugin';

// ============================================================
// Declare the global bridge injected by JUCE C++
// ============================================================
declare global {
  interface Window {
    __chromaDelay?: {
      sendMessage: (msg: object) => void;
      _receiveParam: (paramID: string, value: number) => void;
      _paramHandler?: (paramID: string, value: number) => void;
    };
    __chromaDelayParamHandler?: (paramID: string, value: number) => void;
  }
}

// ============================================================
// Bridge hook
// ============================================================
export function usePluginBridge() {
  const [params, setParams] = useState<PluginState>(defaultState);
  const paramsRef = useRef<PluginState>(defaultState);

  // ---- Install the receive handler so C++ can call us ----
  useEffect(() => {
    window.__chromaDelayParamHandler = (paramID: string, value: number) => {
      setParams(prev => {
        const next = { ...prev, [paramID]: value };
        paramsRef.current = next;
        return next;
      });
    };
    return () => { window.__chromaDelayParamHandler = undefined; };
  }, []);

  // ---- Send normalised value to JUCE ----
  const setParam = useCallback((paramID: string, normalisedValue: number) => {
    // Optimistic update
    setParams(prev => {
      const next = { ...prev, [paramID]: normalisedValue };
      paramsRef.current = next;
      return next;
    });

    if (window.__chromaDelay) {
      window.__chromaDelay.sendMessage({
        type: 'setParam',
        paramID,
        value: normalisedValue,
      });
    } else {
      // Dev mode without plugin — just log
      console.debug('[Bridge] setParam', paramID, normalisedValue);
    }
  }, []);

  // ---- Tap tempo ----
  const tapTempo = useCallback(() => {
    if (window.__chromaDelay) {
      window.__chromaDelay.sendMessage({ type: 'tapTempo' });
    }
  }, []);

  return { params, setParam, tapTempo, paramsRef };
}

// ============================================================
// Utility: normalise a raw value to 0..1 for a given linear range
// ============================================================
export function normLin(value: number, min: number, max: number): number {
  return Math.max(0, Math.min(1, (value - min) / (max - min)));
}

export function denormLin(norm: number, min: number, max: number): number {
  return min + norm * (max - min);
}

// Log-scale normalisation (good for frequency, delay time)
export function normLog(value: number, min: number, max: number): number {
  return Math.max(0, Math.min(1,
    (Math.log(value) - Math.log(min)) / (Math.log(max) - Math.log(min))
  ));
}

export function denormLog(norm: number, min: number, max: number): number {
  return Math.exp(Math.log(min) + norm * (Math.log(max) - Math.log(min)));
}
