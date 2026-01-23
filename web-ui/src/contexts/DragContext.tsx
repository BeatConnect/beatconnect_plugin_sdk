/**
 * DragContext - Tracks whether any control is being dragged
 * Used to disable hover effects on other elements during drag
 */

import { createContext, useContext, useState, useCallback, ReactNode } from 'react';

interface DragContextType {
  isDragging: boolean;
  setDragging: (dragging: boolean) => void;
}

const DragContext = createContext<DragContextType>({
  isDragging: false,
  setDragging: () => {},
});

export function DragProvider({ children }: { children: ReactNode }) {
  const [isDragging, setIsDragging] = useState(false);

  const setDragging = useCallback((dragging: boolean) => {
    setIsDragging(dragging);
  }, []);

  return (
    <DragContext.Provider value={{ isDragging, setDragging }}>
      {children}
    </DragContext.Provider>
  );
}

export function useDragContext() {
  return useContext(DragContext);
}
