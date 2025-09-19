import { PrismaPg } from '@prisma/adapter-pg';
import { PrismaClient } from '@prisma/client';
import { Pool } from 'pg';

import { getEnv, isDevOrTest } from './env';

let prisma: PrismaClient | undefined;
export const getPrisma = () => {
  if (prisma) {
    return prisma;
  }

  const env = getEnv();

  const sslExtra = isDevOrTest() ? {} : { ssl: { rejectUnauthorized: false } };

  const pool = new Pool({
    connectionString: env.DATABASE_URL,
    ...sslExtra,
  });
  prisma = new PrismaClient({
    adapter: new PrismaPg(pool),
    log:
      env.DB_DEBUG === 'true'
        ? [
            {
              emit: 'stdout',
              level: 'query',
            },
            {
              emit: 'stdout',
              level: 'error',
            },
            {
              emit: 'stdout',
              level: 'info',
            },
            {
              emit: 'stdout',
              level: 'warn',
            },
          ]
        : [],
  });

  return prisma;
};
