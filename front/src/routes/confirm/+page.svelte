<script lang="ts">
	import { PUBLIC_API_URL } from '$env/static/public';
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
	import Spinner from '$lib/components/Spinner.svelte';

	let status = $state<'consent' | 'loading' | 'success' | 'error'>('consent');
	let errorMessage = $state('');
	let consent = $state(false);

	const confirmSubscribe = async () => {
		const urlParams = new URLSearchParams(window.location.search);
		const token = urlParams.get('token');

		if (!token) {
			status = 'error';
			errorMessage = 'No token provided';
			return;
		}

		try {
			const response = await fetch(`${PUBLIC_API_URL}/api/auth/subscribe/confirm`, {
				method: 'POST',
				body: JSON.stringify({ token, trackerPixelConsent: consent })
			});

			const result = await response.json();

			if (!response.ok) {
				throw Error(result?.message ?? 'An error occurred');
			}

			status = 'success';
		} catch (e) {
			status = 'error';
			errorMessage = e instanceof Error ? e.message : 'An error occurred';
		}
	};
</script>

<Card centered>
	{#if status === 'consent'}
		<h1>Confirm your subscription</h1>
		<p>
			Before going futher, I need to know if you consent to let me track if you opened the
			newsletters (and only the newsletters). Because we are in a capitalist world, I need some data
			to be able to get some sponsors and earn a little of money to continue this adventure. Despite
			that, this is your right to refuse it of course.
		</p>

		<div class="consent">
			<label for="consent">Do you consent for the pixel tracker on the newsletters mail?</label>
			<input type="checkbox" id="consent" name="consent" bind:checked={consent} />
		</div>

		<Button onclick={confirmSubscribe}>I am ready to receive this amazing newsletter!</Button>
	{:else if status === 'loading'}
		<Spinner text="Confirming your subscription..." />
	{:else if status === 'success'}
		<h1>You have been successfully subscribed!</h1>

		<p>
			Welcome! Thank you so much for your subscription, I hope you will enjoy this newsletter as
			much as I love to make it!
		</p>

		<Button tag="a" href="/">Go back home</Button>
	{:else}
		<h1>Something went wrong</h1>

		<p>{errorMessage}</p>

		<Button tag="a" href="/">Go back home</Button>
	{/if}
</Card>

<style lang="scss">
	.consent {
		display: flex;
		gap: 6px;
		margin-bottom: 12px;
	}
</style>
