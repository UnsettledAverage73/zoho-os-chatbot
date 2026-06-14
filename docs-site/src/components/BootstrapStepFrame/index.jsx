import React from "react";
import styles from "./styles.module.css";

export default function BootstrapStepFrame({
  step = 0,
  title = "Bootstrap visualizer"
}) {
  const src = `/visualizers/bootstrap_animation.html?step=${step}`;

  return (
    <div className={styles.wrapper}>
      <div className={styles.header}>
        <strong>{title}</strong>
        <a href={src} target="_blank" rel="noreferrer">
          Open standalone
        </a>
      </div>
      <iframe
        className={styles.frame}
        src={src}
        title={`${title} step ${step}`}
        loading="lazy"
      />
    </div>
  );
}
