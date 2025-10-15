module.exports = {
  extends: [
    'plugin:@api3/eslint-plugin-commons/universal',
    'plugin:@api3/eslint-plugin-commons/jest',
    'plugin:@api3/eslint-plugin-commons/react',
  ],
  parserOptions: {
    project: ['./tsconfig.json', './tsconfig.node.json'],
  },
  overrides: [
    {
      // This prevents date-fns fns from being used directly (and by extension forces UTC wrapper fns)
      files: ['packages/frontend/**/*'],
      excludedFiles: ['packages/frontend/src/lib/date-fns-utils.ts'],
      rules: {
        'no-restricted-imports': [
          'error',
          {
            name: 'date-fns',
            message: 'Please use the lib/date-fns-utils.ts instead.',
          },
        ],
      },
    },
    {
      files: ['packages/frontend/src/components/ui/**/*.{ts,tsx}'],
      rules: {
        '@typescript-eslint/no-restricted-imports': 'off', // Shadcn UI imports all from React.
      },
    },
    {
      files: ['packages/backend/**/*.ts'],
      rules: {
        'no-restricted-imports': [
          'error',
          {
            name: 'decimal.js',
            message: 'Please use Prisma.Decimal instead.',
          },
        ],
      },
    },
    {
      files: ['packages/frontend/**/*'],
      rules: {
        'lodash/import-scope': 'error'
      }
    }
  ],
  rules: {
    'react/function-component-definition': 'off',
    'react/no-unstable-nested-components': 'off',
    'react/no-array-index-key': 'off',
    'react/destructuring-assignment': 'off',
    'react/jsx-curly-brace-presence': 'off',
    'no-nested-ternary': 'off',
    'functional/no-promise-reject': 'off',
    'sort-keys': 'off',
    'lodash/import-scope': 'off', // Only applicable to frontend
    'lodash/prop-shorthand': 'off',
    'jest/no-hooks': 'off',
    'jest/require-hook': 'off',
  },
};

