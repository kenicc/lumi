import { expect, test } from "@playwright/test";

const D = "/d/LUMI-C3D4E5";

test("lista urządzeń i pulpit z sekcją Dziś", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "Cześć, Ania" })).toBeVisible();
  await page.getByText("Mama").first().click();
  await expect(page).toHaveURL(new RegExp(`${D}$`));
  await expect(page.getByText("bateria").first()).toBeVisible();
  await expect(page.getByRole("heading", { name: "Dziś" })).toBeVisible();
  await expect(page.getByText("Leki na ciśnienie").first()).toBeVisible();
});

test("kontakt: zły numer daje komunikat, poprawny trafia na listę", async ({ page }) => {
  await page.goto(`${D}/kontakty`);
  await page.getByText("Dodaj kontakt").click();
  const form = page.locator("details[open] form");
  await form.locator("input[name=name]").fill("Zenek");
  await form.locator("input[name=number]").fill("abc");
  // walidację robi serwer (ten sam schemat co dla urządzenia)
  await form.getByRole("button", { name: "Dodaj" }).click();
  await expect(page.locator(".flash.err")).toContainText("Numer może mieć tylko cyfry");
  await page.getByText("Dodaj kontakt").click();
  await form.locator("input[name=name]").fill("Zenek");
  await form.locator("input[name=number]").fill("+48 605 600 700");
  await form.getByRole("button", { name: "Dodaj" }).click();
  await expect(page.locator(".flash.ok")).toContainText("Zapisano");
  await expect(page.getByText("+48 605 600 700")).toBeVisible();
});

test("przypomnienie: dodane jest na liście i w planie dnia", async ({ page }) => {
  await page.goto(`${D}/przypomnienia`);
  await page.locator("input[name=text]").fill("Spacer z sąsiadką");
  await page.locator("input[name=time]").fill("23:55");
  await page.getByRole("button", { name: "Dodaj" }).click();
  await expect(page.locator(".flash.ok")).toContainText("Zapisano");
  await expect(page.locator("li", { hasText: "Spacer z sąsiadką" }).first()).toBeVisible();
  await page.goto(D);
  const today = page.locator("section", { has: page.getByRole("heading", { name: "Dziś" }) });
  await expect(today.getByText("Spacer z sąsiadką")).toBeVisible();
});

test("wiadomość: wysłana czeka na przeczytanie", async ({ page }) => {
  await page.goto(`${D}/wiadomosci`);
  await page.locator("input[name=text]").fill("Dzwonię wieczorem");
  await page.getByRole("button", { name: "Wyślij na LUMI" }).click();
  await expect(page.locator(".flash.ok")).toContainText("Wysłano");
  await expect(page.locator("li", { hasText: "Dzwonię wieczorem" })).toContainText("czeka na przeczytanie");
});

test("ekran główny: zmiana przycisku widać w podglądzie i w historii zmian", async ({ page }) => {
  await page.goto(`${D}/ekran`);
  await page.locator("select[name=slot2]").selectOption("contact:basia");
  await page.getByRole("button", { name: "Zapisz" }).click();
  await expect(page.locator(".flash.ok")).toContainText("Zapisano");
  await expect(page.locator(".card").first()).toContainText("Basia");
  await page.goto(D);
  await expect(page.getByText("zmieniła przyciski na ekranie głównym")).toBeVisible();
});

test("API urządzenia bez sekretu jest zamknięte", async ({ request }) => {
  const r = await request.get("/api/device/config", { headers: { "X-Lumi-Device": "LUMI-C3D4E5" } });
  expect(r.status()).toBe(401);
});
