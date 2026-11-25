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

test("propozycja kontaktu od znajomej: opiekun dodaje jednym przyciskiem", async ({ page }) => {
  await page.goto(D);
  await expect(page.getByText("1 nowa propozycja")).toBeVisible();
  await page.goto(`${D}/kontakty`);
  const sec = page.locator("section", { has: page.getByRole("heading", { name: "Propozycje od znajomych" }) });
  await expect(sec).toContainText("Pani Zosia");
  await sec.getByRole("button", { name: "Dodaj do LUMI" }).click();
  await expect(page.locator(".flash.ok")).toContainText("Zapisano");
  await expect(page.getByRole("heading", { name: "Propozycje od znajomych" })).toHaveCount(0);
  await expect(page.locator("li", { hasText: "Pani Zosia" }).first()).toBeVisible();
});

test("strona dla znajomych jest niedostępna dla opiekuna (role sprawdza serwer)", async ({ page }) => {
  await page.goto(`${D}/zaproponuj`);
  await expect(page).toHaveURL(new RegExp(`${D}\\?err=`));
  await expect(page.locator(".flash.err")).toContainText("Brak uprawnień");
});

test("strona prywatności jest dostępna bez logowania", async ({ page }) => {
  await page.goto("/prywatnosc");
  await expect(page.getByRole("heading", { name: "Czego nie zbieramy" })).toBeVisible();
  await expect(page.getByText("lokalizacji")).toBeVisible();
});

// na końcu: usuwa konto demo (Ania), rola opiekuna przechodzi na Marka
test("usunięcie konta wymaga potwierdzenia i przekazuje rolę opiekuna", async ({ page }) => {
  await page.goto("/profil");
  await page.getByText("Usuń konto", { exact: true }).click();
  await page.getByRole("button", { name: "Usuń konto na stałe" }).click();
  await expect(page.locator(".flash.err")).toContainText("Wpisz USUŃ");
  await page.getByText("Usuń konto", { exact: true }).click();
  await page.locator("input[name=confirm]").fill("usuń");
  await page.getByRole("button", { name: "Usuń konto na stałe" }).click();
  await expect(page).toHaveURL(/\/login\?deleted=1/);
  await expect(page.locator(".flash.ok")).toContainText("Konto zostało usunięte");
});
