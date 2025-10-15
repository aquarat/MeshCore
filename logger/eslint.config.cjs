// @ts-check

const { defineConfig, globalIgnores } = require('@eslint/config-helpers');
const js = require('@eslint/js');
const { FlatCompat } = require('@eslint/eslintrc');
const tsPlugin = require('@typescript-eslint/eslint-plugin');
const tsParser = require('@typescript-eslint/parser');
const shopifyPlugin = require('@shopify/eslint-plugin');
const checkFilePlugin = require('eslint-plugin-check-file');
// const deprecationPlugin = require('eslint-plugin-deprecation');
const functionalPlugin = require('eslint-plugin-functional');
const importPlugin = require('eslint-plugin-import');
const lodashPlugin = require('eslint-plugin-lodash');
const unicornPlugin = require('eslint-plugin-unicorn');
const globals = require('globals');

const compat = new FlatCompat({
  baseDirectory: __dirname,
  recommendedConfig: js.configs.recommended,
  allConfig: js.configs.all,
});

// The original file was trying to mix legacy '.eslintrc' format with the new flat config format.
// The main issue was using `extends` and `plugins` arrays with string identifiers directly
// in a flat config object, which caused the "Plugin "" not found" error.
// The fix involves using `FlatCompat` to translate the legacy `extends` configurations
// and properly structuring the plugins and other settings in the flat config array format.

module.exports = defineConfig([
  // 1. Start with the translated 'extends' from the old config.
  // We use the spread operator `...` because compat.extends() returns an array of configs.
  ...compat.extends(
    'eslint:recommended',
    'plugin:@typescript-eslint/recommended',
    'plugin:import/recommended',
    'plugin:import/typescript',
    // 'plugin:unicorn/recommended',
    'plugin:promise/recommended',
    'plugin:lodash/recommended',
  ),

  // 2. Add a new configuration object for your custom rules, plugins, and language options.
  {
    plugins: {
    //   '@shopify': shopifyPlugin,
      '@typescript-eslint': tsPlugin,
      'check-file': checkFilePlugin,
    //   deprecation: deprecationPlugin,
      functional: functionalPlugin,
      import: importPlugin,
      lodash: lodashPlugin,
      unicorn: unicornPlugin,
    },

    languageOptions: {
      parser: tsParser,
      parserOptions: {
        ecmaVersion: 2022,
        sourceType: 'module',
        project: ['./tsconfig.json', './tsconfig.node.json'],
      },
      globals: {
        ...globals.node,
      },
    },

    rules: {
      // Rule overrides from the top of the original file
      'functional/no-promise-reject': 'off',
      'jest/no-hooks': 'off',
      'jest/require-hook': 'off',

      /* Rule definitions and overrides for standard ESLint rules */
      camelcase: 'error',
      curly: ['error', 'multi-line', 'consistent'],
      eqeqeq: 'error',
      'no-await-in-loop': 'off', // Too restrictive, often false yields to more verbose code.
      'no-console': ['error', { allow: ['info', 'groupCollapsed', 'groupEnd'] }],
      'no-constant-condition': 'off', // Writing a "while(true)"" loop is often the most readable way to express the intent.
      'no-fallthrough': 'off', // Does not work well with typescript exhaustive enums.
      'no-inline-comments': 'off',
      'no-lonely-if': 'error',
      'no-nested-ternary': 'error',
      'no-new-wrappers': 'error',
      'no-return-await': 'off', // Superceded by @typescript-eslint/return-await.
      'no-unexpected-multiline': 'off', // Conflicts with prettier.
      'no-unused-expressions': 'off', // Superceded by @typescript-eslint/no-unused-expressions.
      'object-shorthand': 'error',
      'prefer-destructuring': [
        'error',
        {
          array: false, // For arrays it is often confusing to use destructuring.
          object: true,
        },
        {
          enforceForRenamedProperties: false,
        },
      ],
      'prefer-exponentiation-operator': 'error',
      'prefer-named-capture-group': 'error',
      'prefer-object-spread': 'error',
      'prefer-spread': 'error',
      'prefer-template': 'error',
      radix: 'error',
      'sort-keys': [
        'error',
        'asc',
        { caseSensitive: true, natural: true, minKeys: 10, allowLineSeparatedGroups: true },
      ],
      'spaced-comment': [
        'error',
        'always',
        {
          line: {
            markers: ['/'],
          },
        },
      ],

      /* Rules to enforce kebab-case folder structure */
      'check-file/folder-naming-convention': [
        'error',
        {
          '**/': 'KEBAB_CASE',
        },
      ],
      'unicorn/filename-case': [
        'error',
        {
          case: 'kebabCase',
          ignore: [],
        },
      ],

      /* Rule overrides for "unicorn" plugin */
      'unicorn/consistent-function-scoping': 'off', // Disabling due to the rule's constraints conflicting with established patterns, especially in test suites where local helper or mocking functions are prevalent and do not necessitate exports.
      'unicorn/import-style': [
        'error',
        {
          styles: {
            'node:path': { named: true }, // Allows import { join } from 'node:path';
            'node:util': { default: true }, // Allows import util from 'node:util';
          },
        },
      ],
      'unicorn/no-abusive-eslint-disable': 'off', // Already covered by different ruleset.
      'unicorn/no-array-callback-reference': 'error', // Explicitly turned on, because it was initially disabled and "point free" notation was enforced using "functional/prefer-tacit". That said, the point free pattern is dangerous in JS. See: https://github.com/sindresorhus/eslint-plugin-unicorn/blob/main/docs/rules/no-array-callback-reference.md.
      'unicorn/no-array-for-each': 'off', // We use .forEach extensively across the api3dao org and even though this can be solved with --fix and there are benefits, it will generate a lot of friction.
      'unicorn/no-array-reduce': 'off', // We are OK with using reduce occasionally, but I agree with the author that the code using reduce can easily get complex.
      'unicorn/no-nested-ternary': 'off', // This rule is smarter than the standard ESLint rule, but conflicts with prettier so it needs to be turned off. Nested ternaries are very unreadable so it's OK if all of them are flagged.
      'unicorn/no-null': 'off', // We use both null and undefined for representing three state objects. We could use a string union instead, but using combination of null and undefined is less verbose.
      'unicorn/no-object-as-default-parameter': 'off', // Too restrictive. TypeScript can ensure that the default value matches the type.
      'unicorn/no-process-exit': 'off',
      'unicorn/no-useless-undefined': ['error', { checkArguments: false }], // We need to disable "checkArguments", because if a function expects a value of type "T | undefined" the undefined value needs to be passed explicitly.
      'unicorn/prefer-module': 'off', // We use CJS for configuration files and tests. There is no rush to migrate to ESM and the configuration files are probably not yet ready for ESM yet.
      'unicorn/prefer-string-raw': 'off', // We commonly escape \ in strings.
      'unicorn/prefer-top-level-await': 'off',
      'unicorn/prevent-abbreviations': 'off', // This rule reports many false positives and leads to more verbose code.

      /* Rule overrides for "import" plugin */
      'import/no-default-export': 'error',
      'import/no-duplicates': 'error',
      'import/no-named-as-default': 'off',
      'import/no-unresolved': 'off', // Does not accept exports keyword. See: https://github.com/import-js/eslint-plugin-import/issues/1810.
      // 'import/order': ['error', universalImportOrderConfig],

      /* Rule overrides for "@typescript-eslint" plugin */
      '@typescript-eslint/comma-dangle': 'off', // Conflicts with prettier.
      '@typescript-eslint/consistent-type-exports': [
        'error',
        {
          fixMixedExportsWithInlineTypeSpecifier: true,
        },
      ],
      '@typescript-eslint/consistent-type-imports': [
        'error',
        {
          prefer: 'type-imports',
          disallowTypeAnnotations: false, // It is quite common to do so. See: https://typescript-eslint.io/rules/consistent-type-imports/#disallowtypeannotations.
          fixStyle: 'inline-type-imports',
        },
      ],
      '@typescript-eslint/consistent-return': 'off', // Does not play with no useless undefined when function return type is "T | undefined" and does not have a fixer.
      '@typescript-eslint/explicit-function-return-type': 'off', // Prefer inferring types to explicit annotations.
      '@typescript-eslint/explicit-module-boundary-types': 'off', // We export lot of functions in order to test them. Typing them all is not a good idea.
      '@typescript-eslint/indent': 'off', // Conflicts with prettier.
      '@typescript-eslint/init-declarations': 'off', // Too restrictive, TS is able to infer if value is initialized or not. This pattern does not work with declaring a variable and then initializing it conditionally (or later).
      '@typescript-eslint/lines-around-comment': 'off', // Do not agree with this rule.
      '@typescript-eslint/max-params': 'off',
      '@typescript-eslint/member-delimiter-style': 'off', // Conflicts with prettier.
      '@typescript-eslint/member-ordering': 'off', // Does not have a fixer. Also, sometimes it's beneficial to group related members together.
      '@typescript-eslint/naming-convention': 'off',
      '@typescript-eslint/no-confusing-void-expression': [
        'error',
        {
          ignoreArrowShorthand: true, // See: https://typescript-eslint.io/rules/no-confusing-void-expression/#ignorearrowshorthand.
        },
      ],
      '@typescript-eslint/no-dynamic-delete': 'off',
      '@typescript-eslint/no-empty-function': 'off', // Too restrictive, often false yields to more verbose code.
      '@typescript-eslint/no-explicit-any': 'off', // Using "any" is sometimes necessary.
      '@typescript-eslint/no-extra-parens': 'off', // Conflicts with prettier.
      '@typescript-eslint/no-magic-numbers': 'off', // Too restrictive. There is often nothing wrong with inlining numbers.
      '@typescript-eslint/no-misused-promises': [
        'error',
        {
          checksVoidReturn: {
            arguments: false, // It's common to pass async function where one expects a function returning void.
            attributes: false, // It's common to pass async function where one expects a function returning void.
          },
        },
      ],
      '@typescript-eslint/no-non-null-assertion': 'off', // Too restrictive. The inference is often not powerful enough or there is not enough context.
      '@typescript-eslint/no-require-imports': 'off', // We use a similar rule called "@typescript-eslint/no-var-imports" which bans require imports alltogether.
      // '@typescript-eslint/no-restricted-imports': ['error', universalRestrictedImportsConfig],
      '@typescript-eslint/no-shadow': 'off', // It is often valid to shadow variable (e.g. for the lack of a better name).
      '@typescript-eslint/no-type-alias': 'off', // The rule is deprecated and "@typescript-eslint/consistent-type-definitions" is used instead.
      '@typescript-eslint/no-unnecessary-condition': 'off', // Suggests removing useful conditionals for index signatures and arrays. Would require enabling additional strict checks in TS, which is hard to ask.
      '@typescript-eslint/no-unsafe-argument': 'off', // Too restrictive, often false yields to more verbose code.
      '@typescript-eslint/no-unsafe-assignment': 'off', // Too restrictive, often false yields to more verbose code.
      '@typescript-eslint/no-unsafe-member-access': 'off', // Too restrictive, often false yields to more verbose code.
      '@typescript-eslint/no-unsafe-return': 'off', // Too restrictive, often false yields to more verbose code.
      '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_', varsIgnorePattern: '^_', vars: 'all' }],
      '@typescript-eslint/no-use-before-define': 'off', // Too restrictive, does not have a fixer and is not important.
      '@typescript-eslint/no-var-requires': 'off',
      '@typescript-eslint/object-curly-spacing': 'off', // Conflicts with prettier.
      '@typescript-eslint/prefer-nullish-coalescing': [
        'error',
        {
          ignoreConditionalTests: true, // Its more intuitive to use logical operators in conditionals.
        },
      ],
      '@typescript-eslint/prefer-readonly-parameter-types': 'off', // Too restrictive, often false yields to more verbose code.
      '@typescript-eslint/quotes': 'off', // Conflicts with prettier.
      '@typescript-eslint/semi': 'off', // Conflicts with prettier.
      '@typescript-eslint/space-before-function-paren': 'off', // Conflicts with prettier.
      '@typescript-eslint/strict-boolean-expressions': 'off', // While the rule is reasonable, it is often convenient and intended to just check whether the value is not null or undefined. Enabling this rule would make the code more verbose. See: https://typescript-eslint.io/rules/strict-boolean-expressions/
      '@typescript-eslint/unbound-method': 'off', // Reports issues for common patterns in tests (e.g. "expect(logger.warn)..."). Often the issue yields false positives.
      '@typescript-eslint/use-unknown-in-catch-callback-variable': 'off',

      /* Rule overrides for "functional" plugin */
      'functional/no-classes': 'error', // Functions are all we need.
      'functional/no-try-statements': 'error', // Use go utils instead.
      'functional/prefer-tacit': 'off', // The rule is dangerous. See: https://github.com/sindresorhus/eslint-plugin-unicorn/blob/main/docs/rules/no-array-callback-reference.md.

      /* Overrides for "lodash" plugin */
      'lodash/import-scope': ['error', 'member'], // We prefer member imports in node.js code. This is not recommended for FE projects, because lodash can't be tree shaken (written in CJS not ESM). This rule should be overridden for FE projects (and we do so in React ruleset).
      'lodash/path-style': 'off', // Can potentially trigger TS errors. Both variants have use cases when they are more readable.
      'lodash/prefer-immutable-method': 'off',
      'lodash/prefer-lodash-method': 'off', // Disagree with this rule. Using the native method is often simpler.
      'lodash/prop-shorthand': 'off',

      /* Rule overrides for other plugins and rules */
      // This rule unfortunately does not detect deprecated properties. See:
      // https://github.com/gund/eslint-plugin-deprecation/issues/13/
    //   'deprecation/deprecation': 'error',

      /* Select rules from Shopify */
    //   '@shopify/prefer-early-return': 'error',
    //   '@shopify/prefer-module-scope-constants': 'error',
    },
  },

  // 3. Add the global ignores configuration object at the end of the array.
  globalIgnores([
    '!.vscode/extensions.json',
    '**/.build',
    '**/.DS_Store',
    '**/.env',
    '**/.eslintcache',
    '**/.history',
    '**/.idea',
    '**/.log',
    '**/.tsbuildinfo',
    '**/.vite',
    '**/.vscode',
    '**/*.log',
    '**/*.njsproj',
    '**/*.ntvs*',
    '**/*.sln',
    '**/*.suo',
    '**/*.sw?',
    '**/build',
    '**/coverage',
    '**/dist',
    '**/logs',
    '**/node_modules',
    '**/**/node_modules/',
    '**/*.scratch',
    '**/tmp',
    'packages/backend/wrangler.toml',
    'packages/backend/.wrangler',
    '**/generated-reports',
    '**/*payout-data.json',
    '**/*payout-report.pdf',
    'packages/frontend/test-results',
    'packages/frontend/playwright-report',
    'packages/frontend/blob-report',
    'packages/frontend/playwright/.cache',
    '**/mcp.json',
    '**/.roo',
    '**/.cline',
  ]),
]);
