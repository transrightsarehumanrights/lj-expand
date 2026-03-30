import React from 'react';
import ReactMarkdown from 'react-markdown';
import styles from './NamespaceDocs.module.css';

function Markdown({ children }) {
  if (!children) return null;
  return <ReactMarkdown>{children}</ReactMarkdown>;
}

function OptionDoc({ option }) {
  return (
    <div className={styles.functionDoc}>
      <h3 id={option.name}>
        <code>{option.name}</code>
      </h3>
      <Markdown>{option.description}</Markdown>
    </div>
  );
}

export default function CliOptionsDocs({ data }) {
  return (
    <div>
      <p className={styles.namespaceDescription}><ReactMarkdown>{data.description}</ReactMarkdown></p>
      {data.options?.map(option => (
        <OptionDoc key={option.name} option={option} />
      ))}
    </div>
  );
}
