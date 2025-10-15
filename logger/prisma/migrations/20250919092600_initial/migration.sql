-- CreateTable
CREATE TABLE "public"."mesh_packets" (
    "id" UUID NOT NULL,
    "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "rawHex" TEXT NOT NULL,
    "rawLength" INTEGER NOT NULL,
    "deviceMac" TEXT,
    "rssi" INTEGER,
    "snr" DOUBLE PRECISION,
    "routeType" INTEGER NOT NULL,
    "routeTypeName" TEXT NOT NULL,
    "payloadType" INTEGER NOT NULL,
    "payloadTypeName" TEXT NOT NULL,
    "payloadVersion" INTEGER NOT NULL,
    "transportCode1" INTEGER,
    "transportCode2" INTEGER,
    "pathLength" INTEGER NOT NULL,
    "path" TEXT[],
    "payloadLength" INTEGER NOT NULL,
    "payloadRaw" TEXT NOT NULL,
    "payloadParsed" JSONB,
    "readableText" TEXT,

    CONSTRAINT "mesh_packets_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE INDEX "mesh_packets_createdAt_idx" ON "public"."mesh_packets"("createdAt");

-- CreateIndex
CREATE INDEX "mesh_packets_payloadType_idx" ON "public"."mesh_packets"("payloadType");

-- CreateIndex
CREATE INDEX "mesh_packets_deviceMac_idx" ON "public"."mesh_packets"("deviceMac");

-- CreateIndex
CREATE INDEX "mesh_packets_payloadTypeName_idx" ON "public"."mesh_packets"("payloadTypeName");
