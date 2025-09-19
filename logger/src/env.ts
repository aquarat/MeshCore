import { join } from 'node:path';

import { goSync } from '@api3/promise-utils';
import dotenv from 'dotenv';
import { z } from 'zod';

export const envSchema = z.object({
  DATABASE_URL: z.string().url(),
  NODE_ENV: z.enum(['development', 'production', 'test']),
  DB_DEBUG: z.string().optional(),
});

export type Env = z.infer<typeof envSchema>;

let env: Env | undefined;

export function getEnv(): Env {
  if (env) return env;

  const envPath = join(__dirname, '../.env');
  const processEnv = {};
  const loadEnvResult = goSync(() => dotenv.config({ path: envPath, processEnv }));
  if (loadEnvResult.success) {
    // Can't use logger yet as it relies on getEnv
    console.info(`[Backend] Loaded .env file: ${envPath}`);
  } else {
    console.info(`[Backend] Failed to load .env file - this is expected in production`);
  }

  const parseResult = envSchema.safeParse(processEnv);
  if (!parseResult.success) {
    throw new Error(`[Backend] Invalid environment variables:\n, ${JSON.stringify(parseResult.error.format())}`);
  }

  env = parseResult.data;

  return env;
}

export const isDevOrTest = () => getEnv().NODE_ENV === 'development' || getEnv().NODE_ENV === 'test';
